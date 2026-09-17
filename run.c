#include <ntifs.h>
#include <ntddk.h>
#include <intrin.h>

struct segment {
    unsigned long long long_base_addr;
    unsigned long long long_offset;
    unsigned int legacy_base_addr;
    unsigned int legacy_offset;
    unsigned int limit;
    unsigned short selector;
    unsigned short parameter_count;
    unsigned char type;
    unsigned char S;
    unsigned char DPL;
    unsigned char P;
    unsigned char AVL;
    unsigned char L;
    unsigned char D;
    unsigned char G;
    unsigned char IST;
};

int gdt_long_set(void** gdt_p,unsigned int num,unsigned long long head,unsigned long long tail){
    unsigned long long* gdt = (unsigned long long*)*gdt_p;
    if (num < 2 || num > 511) {
        return -1;
    }
    if (gdt[num + 1] != 0) {
        return -2;
    }
    gdt[num] = tail;
    gdt[num + 1] = head;
    return 0;
}

int gdt_set(void** gdt_p, unsigned int num, unsigned long long descriptor) {
    unsigned long long* gdt = (unsigned long long*)*gdt_p;
    if (num < 2 || num > 511) {
        return -1;
    }
    gdt[num] = descriptor;
    return 0;
}

int gdt(void** gdt_p, unsigned int selector, struct segment* segment_p) {
    struct segment segment = *segment_p;
    if (segment.P == 0) {
        return -1;
    }
    int num = selector >> 3;
    if (num < 2 || num > 511) {
        return -2;
    }
    if (segment.long_base_addr != 0 && segment.legacy_base_addr != 0) {
        return -3;
    }
    int ret = 0;
    if (segment.legacy_base_addr != 0) {
        if (segment.selector != 0) {
            return -4;
        }
        if (segment.L == 1 && segment.D == 1) {
            return -5;
        }
        unsigned int tail = (segment.limit & 0xFFFF) + ((segment.legacy_base_addr & 0xFFFF) << 16);
        unsigned short head_tail = ((segment.legacy_base_addr >> 16) & 0xFF) + ((segment.type & 0xF) << 8) + ((segment.S & 1) << 12) + ((segment.DPL & 3) << 13) + ((segment.P & 1) << 15);
        unsigned int head_head = ((segment.limit >> 16) & 0xF) + ((segment.AVL & 1) << 4) + ((segment.L & 1) << 5) + ((segment.D & 1) << 6) + ((segment.G & 1) << 7) + ((segment.legacy_base_addr >> 24 & 0xFF) << 8);
        unsigned long long head = (head_head << 16) | head_tail;
        unsigned long long descriptor = (head << 32) | tail;
        ret = gdt_set(gdt_p, num, descriptor);
        if (ret != 0) {
            return ret-20;
        }
        return 0;
    }
    if (segment.long_base_addr != 0) {
        if (segment.selector != 0) {
            return -4;
        }
        if (segment.S != 0) {
            return -7;
        }
        unsigned int tail = (segment.limit & 0xFFFF) + ((segment.long_base_addr & 0xFFFF) << 16);
        unsigned short head_tail = ((segment.long_base_addr >> 16) & 0xFF) + ((segment.type & 0xF) << 8) + ((segment.S & 1) << 12) + ((segment.DPL & 3) << 13) + ((segment.P & 1) << 15);
        unsigned int head_head = ((segment.limit >> 16) & 0xF) + ((segment.AVL & 1) << 4) + ((segment.G & 1) << 7) + ((segment.legacy_base_addr >> 24 & 0xFF) << 8);
        unsigned long long head = (head_head << 16) | head_tail;
        unsigned long long tail_descriptor = (head << 32) | tail;
        unsigned long long head_descriptor = segment.long_base_addr >> 32;
        ret = gdt_long_set(gdt_p, num, head_descriptor, tail_descriptor);
        if (ret != 0) {
            return ret - 20;
        }
        return 0;
    }
    if (segment.legacy_base_addr == 0 && segment.long_base_addr == 0) {
        if (segment.selector == 0) {
            return -6;
        }
        if (segment.S != 0) {
            return -7;
        }
        if (segment.legacy_offset != 0) {
            if (segment.type != 0x4 && segment.type != 0xC) {
                if (segment.parameter_count != 0) {
                    return -8;
                }
            }
            unsigned int tail = (segment.legacy_offset & 0xFFFF) + (segment.selector << 16);
            unsigned short head_tail = (segment.parameter_count & 0x1F) + ((segment.type & 0xF) << 8) + ((segment.S & 1) << 12) + ((segment.DPL & 3) << 13) + ((segment.P & 1) << 15);
            unsigned int head_head = segment.legacy_offset >> 16;
            unsigned long long head = (head_head << 16) | head_tail;
            unsigned long long descriptor = (head << 32) | tail;
            ret = gdt_set(gdt_p, num, descriptor);
            if (ret != 0) {
                return ret - 20;
            }
            return 0;
        }
        if (segment.long_offset != 0) {
            if (segment.type < 0xE) {
                if (segment.IST != 0) {
                    return -10;
                }
            }
            unsigned int tail = (segment.long_offset & 0xFFFF) + (segment.selector << 16);
            unsigned short head_tail = (segment.IST & 7) + ((segment.type & 0xF) << 8) + ((segment.S & 1) << 12) + ((segment.DPL & 3) << 13) + ((segment.P & 1) << 15);
            unsigned int head_head = (segment.long_offset >> 16) & 0xFFFF;
            unsigned long long head = (head_head << 16) | head_tail;
            unsigned long long tail_descriptor = (head << 32) | tail;
            unsigned long long head_descriptor = segment.long_offset >> 32;
            ret = gdt_long_set(gdt_p, num, head_descriptor, tail_descriptor);
            if (ret != 0) {
                return ret - 20;
            }
            return 0;
        }
    }
    return -100;
}

int idt(void** idt_p, unsigned int vector , struct segment* segment_p) {
    int index = (vector * 2) << 3;
    int ret = gdt(idt_p, index, segment_p);
    return ret;
}

int legacy_idt(void** idt_p, unsigned int vector, struct segment* segment_p) {
    int index = vector << 3;
    int ret = gdt(idt_p, index, segment_p);
    return ret;
}

void DriverUnload(struct _DRIVER_OBJECT* DriverObject) {
    (void)DriverObject;
}

int DriverEntry(struct _DRIVER_OBJECT* DriverObject, struct _UNICODE_STRING* RegistryPath) {
    (void)RegistryPath;
    int err = 0;
    // 检查SVM支持
    int cpu_info[4] = { 0 };
    __cpuid(cpu_info, 0x80000001);
    if (!(cpu_info[2] & 1 << 2)) {
        err = -1;
        goto go_out;
    }

    // 检查NPT支持
    __cpuid(cpu_info, 0x8000000A);
    if (!(cpu_info[3] & 1 << 0)) {
        err = -4;
        goto go_out;
    }

    // 检查BIOS锁
    unsigned long long cr_vm = __readmsr(0xC0010114);
    if (cr_vm & (1 << 4)) {  // bit4=0表示SVM可用，bit4=1表示被锁
        err = -3;
        goto go_out;
    }

    // 开启SVM
    unsigned long long vmxe = __readmsr(0xC0000080);
    vmxe |= (1 << 12);  // SVME
    __writemsr(0xC0000080, vmxe);


    // 分配VMCB
    PHYSICAL_ADDRESS maxmalloc;
    maxmalloc.QuadPart = (unsigned long long) - 1;
    void* malloc = MmAllocateContiguousMemory(4096, maxmalloc);
    if (!malloc) {
        err = -1;
        goto go_out;
    }
    memset(malloc, 0, 4096);

    // 设置RIP
    void* rip_run_in = MmAllocateContiguousMemory(4096, maxmalloc);
    if (!rip_run_in) {
        err = -1;
        goto go_out;
    }
    memset(rip_run_in, 0, 4096);
    *(unsigned char*)((char*)rip_run_in + 0) = 0xF4;        //rip——hlt机器码
    unsigned long long rip_addr = MmGetPhysicalAddress(rip_run_in).QuadPart;

    *(unsigned long long*)((char*)malloc + 0x178 + 0x400) = 0x1000;     //设置rip  0x1000:0 0 0 1

    // 设置CS
    *(unsigned short*)((char*)malloc + 0x10 + 0x400) = 0x10;
    *(unsigned short*)((char*)malloc + 0x12 + 0x400) = 0xA9A;  //bit0 -> type :: S DPL P :: AVL L D/B G :: Reserved  -> bit15
    *(unsigned int*)((char*)malloc + 0x14 + 0x400) = 0xFFFFF;
    *(unsigned int*)((char*)malloc + 0x18 + 0x400) = 0x12345678;

    // 设置SS
    *(unsigned short*)((char*)malloc + 0x20 + 0x400) = 0x28;
    *(unsigned short*)((char*)malloc + 0x22 + 0x400) = 0x896;
    *(unsigned int*)((char*)malloc + 0x24 + 0x400) = 0xFFFFF;
    *(unsigned int*)((char*)malloc + 0x28 + 0x400) = 0x12345678;

    // 设置ES
    *(unsigned short*)((char*)malloc + 0x0 + 0x400) = 0x20;
    *(unsigned short*)((char*)malloc + 0x2 + 0x400) = 0x892;
    *(unsigned int*)((char*)malloc + 0x4 + 0x400) = 0xFFFFF;
    *(unsigned int*)((char*)malloc + 0x8 + 0x400) = 0x12345678;

    // 设置DS
    *(unsigned short*)((char*)malloc + 0x30 + 0x400) = 0x20;
    *(unsigned short*)((char*)malloc + 0x32 + 0x400) = 0x892;
    *(unsigned int*)((char*)malloc + 0x34 + 0x400) = 0xFFFFF;
    *(unsigned int*)((char*)malloc + 0x38 + 0x400) = 0x12345678;

    // 设置CR0
    unsigned long long cr0 = 0x80000021;
    *(unsigned long long*)((char*)malloc + 0x158 + 0x400) = cr0;

    // 设置EFER
    *(unsigned long long*)((char*)malloc + 0xD0 + 0x400) = 0x1100;

    // 设置CR4
    unsigned long long cr4 = 0x20;
    *(unsigned long long*)((char*)malloc + 0x148 + 0x400) = cr4;

    // 设置RFLAGS
    *(unsigned long long*)((char*)malloc + 0x170 + 0x400) = 0x2;
  
    // 设置CR3
    void* cr3_virtual = MmAllocateContiguousMemory(4096, maxmalloc);
    if (!cr3_virtual) {
        return -1;
    }
    memset(cr3_virtual, 0, 4096);
    unsigned long long cr3_physical = MmGetPhysicalAddress(cr3_virtual).QuadPart;

    void* pdpt_virtual = MmAllocateContiguousMemory(4096, maxmalloc);
    if (!pdpt_virtual) {
        return -1;
    }
    memset(pdpt_virtual, 0, 4096);
    unsigned long long pdpt_physical = MmGetPhysicalAddress(pdpt_virtual).QuadPart;

    void* pd_virtual = MmAllocateContiguousMemory(4096, maxmalloc);
    if (!pd_virtual) {
        return -1;
    }
    memset(pd_virtual, 0, 4096);
    unsigned long long pd_physical = MmGetPhysicalAddress(pd_virtual).QuadPart;

    void* pt_virtual = MmAllocateContiguousMemory(4096, maxmalloc);
    if (!pt_virtual) {
        return -1;
    }
    memset(pt_virtual, 0, 4096);
    unsigned long long pt_physical = MmGetPhysicalAddress(pt_virtual).QuadPart;

    void* rsp_virtual = MmAllocateContiguousMemory(4096, maxmalloc);
    if (!rsp_virtual) {
        return -1;
    }
    memset(rsp_virtual, 0, 4096);
    unsigned long long rsp_physical = MmGetPhysicalAddress(rsp_virtual).QuadPart;

    void* gdt_virtual = MmAllocateContiguousMemory(4096, maxmalloc);
    if (!gdt_virtual) {
        return -1;
    }
    memset(gdt_virtual, 0, 4096);
    unsigned long long gdt_physical = MmGetPhysicalAddress(gdt_virtual).QuadPart;

    void* idt_virtual = MmAllocateContiguousMemory(4096, maxmalloc);
    if (!idt_virtual) {
        return -1;
    }
    memset(idt_virtual, 0, 4096);
    unsigned long long idt_physical = MmGetPhysicalAddress(idt_virtual).QuadPart;
    
    void* tss_virtual = MmAllocateContiguousMemory(4096, maxmalloc);
    if (!tss_virtual) {
        return -1;
    }
    memset(tss_virtual, 0, 4096);
    unsigned long long tss_physical = MmGetPhysicalAddress(tss_virtual).QuadPart;
   *(unsigned long long*)cr3_virtual = pdpt_physical | 3;
    *(unsigned long long*)pdpt_virtual = pd_physical | 3;
    *(unsigned long long*)pd_virtual = pt_physical | 3;

    *(unsigned long long*)((char*)pt_virtual + 8 * 1) = rip_addr | 3;
    *(unsigned long long*)((char*)pt_virtual + 8 * 2) = rsp_physical | 3;
    *(unsigned long long*)((char*)pt_virtual + 8 * 3) = gdt_physical | 3;
    *(unsigned long long*)((char*)pt_virtual + 8 * 4) = idt_physical | 3;
    *(unsigned long long*)((char*)pt_virtual + 8 * 5) = tss_physical | 3;

    *(unsigned long long*)((char*)malloc + 0x150 + 0x400) = cr3_physical;

    // 设置rsp
    *(unsigned long long*)((char*)malloc + 0x1D8 + 0x400) = 0x3000;

    // attrib:TYPE S DPL P AVL L D/B G  bit12-15:Reserved
    // 配置GDTR 
    struct segment long_kernel_code = { 0 };    // 内核代码段 attrib：0xA9A
    long_kernel_code.legacy_base_addr = 0x12345678;
    long_kernel_code.DPL = 0;
    long_kernel_code.D = 0;
    long_kernel_code.L = 1;
    long_kernel_code.P = 1;
    long_kernel_code.type = 0xA;
    long_kernel_code.S = 1;
    long_kernel_code.G = 1;
    long_kernel_code.limit = 0xFFFFF;
    
    struct segment long_kernel_data = { 0 };    // 内核数据段 attrib：0x892
    long_kernel_data.legacy_base_addr = 0x12345678;
    long_kernel_data.DPL = 0;
    long_kernel_data.D = 0;
    long_kernel_data.P = 1;
    long_kernel_data.type = 2;
    long_kernel_data.S = 1;
    long_kernel_data.G = 1;
    long_kernel_data.limit = 0xFFFFF;

    struct segment long_ss_data = { 0 };        // attrib：0x896
    long_ss_data.legacy_base_addr = 0x12345678;
    long_ss_data.DPL = 0;
    long_ss_data.D = 0;
    long_ss_data.P = 1;
    long_ss_data.type = 6;
    long_ss_data.S = 1;
    long_ss_data.G = 1;
    long_ss_data.limit = 0xFFFFF;

    gdt(&gdt_virtual, 0x10, &long_kernel_code);
    gdt(&gdt_virtual, 0x20, &long_kernel_data);
    gdt(&gdt_virtual, 0x28, &long_ss_data);

    *(unsigned int*)((char*)malloc + 0x64 + 0x400) = 0xFFFF;
    *(unsigned long long*)((char*)malloc + 0x68 + 0x400) = 0x3000;

    // 配置IDTR
    struct segment idte = { 0 };
    idte.selector = 0x10;
    idte.IST = 0;
    idte.long_offset = 0x1000;
    idte.P = 1;
    idte.DPL = 0;
    idte.S = 0;
    idte.type = 0xE;

    idt(&idt_virtual, 13, &idte);
    idt(&idt_virtual, 14, &idte);

    *(unsigned int*)((char*)malloc + 0x84 + 0x400) = 0xFFFF;
    *(unsigned long long*)((char*)malloc + 0x88 + 0x400) = 0x4000;

   // 配置TR
    struct segment tss = { 0 };
    tss.P = 1;
    tss.DPL = 0;
    tss.type = 9;
    tss.S = 0;
    tss.G = 1;
    tss.limit = 0xFFFFFFFF;
    tss.long_base_addr = 0x5000;

    gdt(&gdt_virtual, 0x30, &tss);

    *(unsigned long long*)((char*)tss_virtual + 4) = 0x3000;   // 内核栈

    *(short*)((char*)malloc + 0x90 + 0x400) = 0x30;
    *(short*)((char*)malloc + 0x92 + 0x400) = 0x889;
    *(unsigned int*)((char*)malloc + 0x94 + 0x400) = 0xFFFFFFFF;
    *(unsigned long long*)((char*)malloc + 0x98 + 0x400) = 0x5000;

    // ASID设置
    *(unsigned int*)((char*)malloc + 0x58) = 0x1;

    // TLB初始策略
    *(char*)((char*)malloc + 0x5C) = 0x1;   //01h 刷新整个TLB

    // 配置拦截位
    *(unsigned int*)((char*)malloc + 0x10) = 0x3 ;    // 必须拦截vmrun否则vmexit退出码为-1  拦截：vmrun vmcall
    *(unsigned int*)((char*)malloc + 0xC) = 0x81000001;  // 拦截 INTR shutdown HLT
    *(unsigned int*)((char*)malloc + 0x8) = 0x40;    // 拦截 #UD

    // 分配Host保存区域
    void* host_save_addr = MmAllocateContiguousMemory(4096, maxmalloc);
    if (!host_save_addr) {
        return -1;
    }
    memset(host_save_addr, 0, 4096);
    unsigned long long host_save_physical = MmGetPhysicalAddress(host_save_addr).QuadPart;
    __writemsr(0xC0010117, host_save_physical);

    // 执行VMRUN
    unsigned long long result_addr_malloc = MmGetPhysicalAddress(malloc).QuadPart;

    _mm_mfence();    // 读写屏障(保证执行顺序是必须的，否则可能处理器停机)

    _disable();      // 关闭 CR4.IF
    __svm_clgi();    // 清除全局中断 防止中断干扰
    __svm_vmrun(result_addr_malloc);
    __svm_stgi();    // vmrun自动清除全局变量，下文必须开启全局变量否则host处理器无法响应中断导致停机
    _enable();       // 开启 CR4.IF

    _mm_mfence();    // 必须加上内存屏障以抵御乱序执行导致的未知现象
    unsigned long long ret_vmm = *(unsigned long long*)((char*)malloc + 0x70);
    DbgPrint("VMEXIT: 0x%llx\n",ret_vmm);

    // 回收内存
    _mm_mfence();
    MmFreeContiguousMemory(pt_virtual);
    MmFreeContiguousMemory(pd_virtual);
    MmFreeContiguousMemory(pdpt_virtual);
    MmFreeContiguousMemory(rip_run_in);
    MmFreeContiguousMemory(gdt_virtual);
    MmFreeContiguousMemory(rsp_virtual);
    MmFreeContiguousMemory(cr3_virtual);
    MmFreeContiguousMemory(host_save_addr);
    MmFreeContiguousMemory(malloc);

go_out:
    DriverObject->DriverUnload = DriverUnload;
    return 0;
}
