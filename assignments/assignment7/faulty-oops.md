# Faulty Module Kernel Oops Analysis

## Command Executed

```bash
echo "hello_world" > /dev/faulty

Unable to handle kernel NULL pointer dereference at virtual address 0000000000000000
Internal error: Oops: 0000000096000045 [#1] SMP
Modules linked in: hello(O) faulty(O) scull(O)
CPU: 0 PID: 186 Comm: sh Tainted: G           O       6.1.44 #1
pc : faulty_write+0x10/0x20 [faulty]
Call trace:
 faulty_write+0x10/0x20 [faulty]
 ksys_write+0x74/0x110
 __arm64_sys_write+0x1c/0x30
 invoke_syscall+0x54/0x130
 el0_svc_common.constprop.0+0x44/0xf0
 do_el0_svc+0x2c/0xc0
 el0_svc+0x2c/0x90
 el0t_64_sync_handler+0xf4/0x120
 el0t_64_sync+0x18c/0x190
Code: d2800001 d2800000 d503233f d50323bf (b900003f)


##Explanation:

This kernel oops is resulted from writing data to the /dev/faulty device node. The error is caused mainly because the faulty kernel module performing a NULL pointer dereference. The crash specifically occured in the faulty_write function.

The part of code which caused the error:
ssize_t faulty_write(struct file *filp, const char __user *buf, size_t count, loff_t *pos)
{
    /* make a simple fault by dereferencing a NULL pointer */
    *(int *)0 = 0;
    return 0;
}

*(int *)0 = 0; is trying to access a NULL pointer which caused this error.
