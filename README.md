# An Overview of Transient Execution Vulnerabilities

## Introduction & Background

With the discovery of Spectre and Meltdown in 2017, a new class of vulnerabilities was born: transient execution vulnerabilities. These are also known as speculative execution vulnerabilities or microarchitectural CPU vulnerabilities. As computer scientists or programmers we are used to working with abstractions, and as security professionals we know that vulnerabilities are often found were abstractions meet implementations. This holds true for transient execution vulnerabilities as well: programs that are perfectly secure under the typical CPU abstraction that assumes deterministic execution are decidedly less so when they meet actual CPU implementations that use out-of-order execution, speculative execution and branch prediction. Since these vulnerabilities depend on specific CPU features, different CPU architectures and models are affected to very different degrees.

In addition to the different Spectre and Meltdown variants, another class of transient execution vulnerabilities was found later, commonly known as microarchitectural data sampling (MDS) or rogue in-flight data load (RIDL). These use various CPU internal buffers to leak data.

In this work, we attempt to give an overview of the different transient execution vulnerabilities that have been found since 2017, including a brief description of the CPU features that are being exploited and the attacker model that is assumed.

TODO:
- Add sentence on the different archs that we look at (ARM64).
- Add sentence on the PoCs.

### CPU features for performance optimization

Since memory access is orders of magnitude slower than modern CPUs, CPU manufacturers have come up with clever performance optimizations to effectively use the time while waiting for memory access operations to complete. These performance optimizations include the CPU features discussed in this section, and without these features many tasks run significantly slower, in some cases even 10x slower. However, it also these optimizations that have lead to the vulnerabilities discussed in this report.

The various Spectre and Meltdown variants in particular rely on out-of-order execution, speculative execution and branch prediction to leak CPU internal information to an attacker. The MDS or RIDL vulnerabilities that were found later additionally rely on various CPU internal buffers to leak secrets across hyperthreads.

In the following subsections, we will shortly outline what these CPU features do and how they work.

#### Out-of-order Execution, Speculative Execution and Branch Prediction

As programmers, we think of CPU instructions as being executed sequentially, one by one, but in practice instructions can be executed out-of-order, aptly named *out-of-order execution*. This happens when one or more previous instructions are waiting to complete, and the following instructions have no data dependency on the previous instructions. Simply put, if instructions don't need to know any of the resulting values of the previous instructions, they are ready to execute and the CPU will put them in a pipeline.

Note that we specifically qualify the instruction dependency to data dependencies, because control flow dependencies might be ignored if speculative execution is allowed. Speculative execution takes the idea of using the waiting time effectively even further, by allowing the execution of branches without knowing if they will be taken or not, i.e., executing them speculatively. This is where branch prediction comes in: based on previous executions, the CPU guesses which branch will be executed next.
The important part is that speculative execution is CPU internal, nothing that is executed speculatively is written back to memory, and if it turns out that a branch was taken in error, the CPU can roll back its internal state to before the branch was taken. However, speculative execution has side-effects such as affecting the CPU caches that are not rolled back, so that some information of the speculated branches can leak through side-channels.

#### Simultaneous Multithreading (SMT) & CPU internal buffers

Another speed optimization that CPU manufacturers introduced to modern architectures is so called *simultaneous multithreading (SMT)*, which makes a single CPU core look like multiple CPUs to the operating system because this allows additional CPU internal optimizations. Intel's implementation of SMT is called *Hyper-Threading Technology (HTT)*. Threads share certain internal resources, such as caches and buffers, which makes it possible for information to leak from one thread to another through side-channels that use the shared resources. In this section we will only highlight a few specific buffers that have been used in MDS/RIDL to date.

TODO: write a description of these buffers.

- Fill buffer.
- Store buffer.
- Load port buffer.
- Uncacheable memory.
- ?


### Relevant Cache Side-Channel Attacks

#### Flush+Reload

#### Evict+Reload

#### ...


### Attacker model(s)

Looking at transient execution vulnerabilities, a relevant question is under which circumstances they can be exploited. Let us establish some terminology first:
- There is a *secret* which the *attacker* wants to leak, for instance the root password hash stored in /etc/shadow.
- The secret is loaded into memory by a *target process*, for instance by an ssh server.
- The attacker can execute an *exploit* that implements a side-channel attack.

In general, the exploit must be executed on the same CPU core or thread as the target process, and in most cases the exploit must also be able to control when the target process loads the secret, for instance by attempting to log into the ssh server.


## Spectre & Meltdown Variants

### Spectre v1 - Bounds Check Bypass (BCB)

### Spectre v2 - Branch Target Injection (BTI)

### Meltdown - Rogue Data Cache Load (RDCL)

When a process running on a multi-user OS (like Linux or Windows) wants to
read a file or send some data over network, it does so through a _system call_
(syscall). It puts the syscall parameters into the CPU registers and executes
a special CPU instruction (`int 0x80` or `sysenter` on x86, `cwc` or `swi`
on ARM), which triggers a cpu interrupt and starts the interrupt handler in
the OS code. The interrupt handler runs with kernel privileges, usually it
can access any memory of any process or talk to any hadware directly—the code
inside the kernel is trusted. This allows the interrupt handler to serve the
syscall request from the user process; for example, if the process requested to
read 10 bytes from some file, the interrupt handler will talk to the HDD, read
the bytes, and copy them into the buffer inside the process memory. Then the
interrupt handler restores the CPU to the state where it has been when the user
process issued the syscall, and continues executing the user process code (after
dropping the kernel privileges, of course).

In order for the kernel code to work, it needs the kernel memory (its code,
private data, drivers to work with devices) to be mapped into the address
space. But we don't want to keep this memory mapped after the interrupt has
terminated (the user might read kernel data!). We could map and unmap the kernel
memory at each syscall, but it's too expensive because of the specifics of how
mapping works. Therefore, operating systems decided to keep it mapped the whole
time, but disable any access (read, write, execute) to that data via special
permission bits which are part of the mapping settings. Changing the permission
bits takes way less time than mapping/unmapping some pages. So the syscall
handling algorithm looks like this:

1. Set the permission bits to make kernel memory readable, writable, executable
2. Run the syscall code
3. Unset the permission bits from step 1
4. Return execution to the user process who issued syscall

The Meltdown vulnerability takes advantage of the speculative execution and
caches to read the kernel memory permission, bypassing the permission bits.

If the user process tries to read the kernel memory, this will cause another
kind of interrupt which basically tells the kernel "this process is reading the
memory it's not allowed to read", and the kernel may either kill the process
or send it a signal to notify that it's not allowed to read that memory. Or
this is how it was supposed to work in theory. In practice, the speculative
execution manages to perform the memory access to the restricted memory and
maybe execute a couple of other instructions that come next. Of course, after
the CPU sees that the memory is not allowed to be read, it will roll back the
speculatively executed operations and return the CPU registers to the state they
had before. But it will not roll back the cache states.



### Spectre-NG v3a - Rogue System Register Read (RSRR)

### Spectre-NG - Lazy FP State Restore

### Spectre-NG v1.1 - Bounds Check Bypass Store (BCBS)

### Spectre-NG v4 - Speculative Store Bypass (SSB)

### SpectreRSB - Return Mispredict

### Spectre SWAPGS


## Foreshadow - L1 Terminal Fault

### Foreshadow

### Foreshadow-OS

### Foreshadow-VMM


## Microarchitectural Data Sampling

### RIDL - Zombieload - Microarchitectural Fill Buffer Data Sampling (MFBDS)

### RIDL - Microarchitectural Load Port Data Sampling (MLPDS)

### RIDL - Microarchitectural Data Sampling Uncacheable memory (MDSUM)

### RIDL - Fallout - Microarchitectural Store Buffer Data Sampling (MSBDS)

### RIDL - Zombieload v2 - Transactional Asynchronous Abort (TAA)

### RIDL - CacheOut - L1D Eviction Sampling (L1DES)

### RIDL - Vector Register Sampling (VRS)

### CROSSTalk - Special Register Buffer Data Sampling (SRBDS)


## Load-Value Injection

