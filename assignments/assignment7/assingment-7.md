# Oops fault analysis


## Scenario
Writing to /dev/faulty causes the kernel to crash.

The kernel output:

    Unable to handle kernel NULL pointer dereference at virtual address 0000000000000000
    Mem abort info:
      ESR = 0x96000045
      EC = 0x25: DABT (current EL), IL = 32 bits
      SET = 0, FnV = 0
      EA = 0, S1PTW = 0
      FSC = 0x05: level 1 translation fault
    Data abort info:
      ISV = 0, ISS = 0x00000045
      CM = 0, WnR = 1
    user pgtable: 4k pages, 39-bit VAs, pgdp=000000004260d000
    [0000000000000000] pgd=0000000000000000, p4d=0000000000000000, pud=0000000000000000
    Internal error: Oops: 96000045 [#1] SMP
    Modules linked in: hello(O) faulty(O) scull(O)
    CPU: 0 PID: 158 Comm: sh Tainted: G           O      5.15.18 #1
    Hardware name: linux,dummy-virt (DT)
    pstate: 80000005 (Nzcv daif -PAN -UAO -TCO -DIT -SSBS BTYPE=--)
    pc : faulty_write+0x14/0x20 [faulty]
    lr : vfs_write+0xa8/0x2b0
    sp : ffffffc008d1bd80
    x29: ffffffc008d1bd80 x28: ffffff8002690000 x27: 0000000000000000
    x26: 0000000000000000 x25: 0000000000000000 x24: 0000000000000000
    x23: 0000000040000000 x22: 000000000000000b x21: 00000055725920f0
    x20: 00000055725920f0 x19: ffffff8002659000 x18: 0000000000000000
    x17: 0000000000000000 x16: 0000000000000000 x15: 0000000000000000
    x14: 0000000000000000 x13: 0000000000000000 x12: 0000000000000000
    x11: 0000000000000000 x10: 0000000000000000 x9 : 0000000000000000
    x8 : 0000000000000000 x7 : 0000000000000000 x6 : 0000000000000000
    x5 : 0000000000000001 x4 : ffffffc0006f7000 x3 : ffffffc008d1bdf0
    x2 : 000000000000000b x1 : 0000000000000000 x0 : 0000000000000000
    Call trace:
     faulty_write+0x14/0x20 [faulty]
     ksys_write+0x68/0x100
     __arm64_sys_write+0x20/0x30
     invoke_syscall+0x54/0x130
     el0_svc_common.constprop.0+0x44/0xf0
     do_el0_svc+0x40/0xa0
     el0_svc+0x20/0x60
     el0t_64_sync_handler+0xe8/0xf0
     el0t_64_sync+0x1a0/0x1a4
    Code: d2800001 d2800000 d503233f d50323bf (b900003f) 
    ---[ end trace 4a07e649f386bc2f ]---

## Analysis
So, what does the kernel dump tell us.
First line says that the crash is caused due to dereferencing a null pointer.

The call trace shows that the program counter PC points to faulty_write+0x14(bytes).
Looking into the source, we see that same line shown in the course video

    *(int *)0 = 0;

