# An Overview of Transient Execution Vulnerabilities

## Introduction & Background

With the discovery of Spectre and Meltdown in 2017, a new class of vulnerabilities was born: transient execution vulnerabilities. These are also known as speculative execution vulnerabilities or microarchitectural CPU vulnerabilities. As computer scientists or programmers we are used to working with abstractions, and as security professionals we know that vulnerabilities are often found were abstractions meet implementations. This holds true for transient execution vulnerabilities as well: programs that are perfectly secure under the typical CPU abstraction that assumes deterministic execution are decidedly less so when they meet actual CPU implementations that use out-of-order execution, speculative execution and branch prediction. Since these vulnerabilities depend on specific CPU features, different CPU architectures and models are affected to very different degrees.

In addition to the different Spectre and Meltdown variants, another class of transient execution vulnerabilities was found later, commonly known as microarchitectural data sampling (MDS) or rogue in-flight data load (RIDL). These use various CPU internal buffers to leak data.

In this work, we attempt to give an overview of the different transient execution vulnerabilities that have been found since 2017, including a brief description of the CPU features that are being exploited and the attacker model that is assumed.

TODO:
- Add sentence on the different archs that we look at (ARM64).
- Add sentence on the PoCs.

### CPU features for performance optimization

Since memory access is orders of magnitude slower than modern CPUs, CPU manufacturers have come up with clever performance optimizations to effectively use the time while waiting for memory access operations to complete. These performance optimizations include the CPU features discussed in this section, and without these features many tasks run significantly slower, in some cases even 10x slower. However, it is also these optimizations that have lead to the vulnerabilities discussed in this report.

The Spectre and Meltdown variants in particular rely on out-of-order execution, speculative execution and branch prediction to leak CPU internal information to an attacker. The MDS or RIDL vulnerabilities that were found later additionally rely on various CPU internal buffers to leak secrets across CPU threads (called hyperthreads in Intel architectures).

In the following subsections, we will shortly outline what these CPU features do and how they work.

#### Out-of-order Execution, Speculative Execution and Branch Prediction

As programmers, we think of CPU instructions as being executed sequentially, one by one, but in practice instructions can be executed out-of-order, aptly named *out-of-order execution*. This happens when one or more previous instructions are waiting to complete, and the following instructions have no data dependency on the previous instructions. Simply put, if instructions don't need to know any of the resulting values of the previous instructions, they are ready to execute and the CPU will put them in a pipeline.

Note that we specifically qualify the instruction dependency to data dependencies, because control flow dependencies might be ignored if speculative execution is allowed. *Speculative execution* takes the idea of using the waiting time effectively even further, by allowing the execution of branches without knowing if they will be taken or not, i.e., executing them speculatively. This is where *branch prediction* comes in: based on previous executions, the CPU guesses which branch will be executed next.
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

Side-channel attacks do not directly attack the application that is being targeted, instead they derive secret information from side-effects of executing that application. The classic example would be a timing side-channel of a poorly implemented password check: if the password is compared character for character, every character that is guessed correctly increases the time the application needs to return with the answer, so an attacker can determine the characters one by one based on the timing of the applications response.

There are many types of side-channel attacks, but in the context of transient execution vulnerabilities, cache side-channel attacks are the most relevant, so we will explain some of the most important concepts here.

#### Basics of CPU caches

As you are probably aware, most desktop & server CPUs have a three-tiered hierarchy of caches, with each cache level being slightly slower and slightly bigger than the previous one: *L1*, *L2*, *L3*. The L1 cache is also typically split into a data cache (*L1d*) and an instruction cache (*L1i*). For our purposes, it is sufficient to know that any value found in cache is significantly faster to retrieve than any value that has to be fetched from main memory, and the difference in timing is clearly observable when measured.

It is also important to remember that caches are organized into so called *cache lines*, a data row of fixed size. A memory address is typically split into three parts to determine its place in the cache: [*tag* | *index* | *offset*]. A *way* indicates how many cache lines can be stored per cache entry. For example, a 2-way set-associative cache can store 2 cache lines at the same index.
TODO: determine if this explanation is needed, and if so, elaborate (explain what tag, index and offset are, etc.).

#### Flush+Reload

The basic idea of a flush+reload attack is simple: the attacker identifies a specific shared memory location they want to monitor, for instance a specific line of code in a shared library, and uses the CPU instruction *clflush* to flush the corresponding cache line from memory, thus guaranteeing that it is no longer in cache. The attacker triggers the loading of the secret by the victim process, and finally they measure the time it takes to reload the flushed cache line. If the access is quick, the attacker knows that the cache line was accessed by the victim process since it was flushed.

TODO: Obviously, timing is very important here. How would this work in practice?

#### Evict+Reload

Evict+reload is a variation of the flush+reload attack which does not use the *clflush* instruction and instead evicts the targeted cache line by accessing data that will replace it. So the main difference is that instead of using a CPU instruction to flush the cache line, the cache line is evicted by loading a different cache line in the same location. This is especially useful when an attacker does not have access to a flush instruction, for instance when performing attacks in a browser.


#### Prime+Probe

In a prime+probe attack, the attacker continously and systematically loads its own known data into every cache line in the cache (that is, accesses every *set* and every *way*) and measures the access time for each cache set, which is known as the priming step. Next, the attacker triggers the victim process, which will load new cache lines and evict some of the attackers data from the cache. Now the probe phase works exactly the same way as the prime phase, i.e., every cache line is accessed, and the access time is measured. If the access time for a particular cache set takes longer than before, the attacker knows that the victim process accessed that particular cache set.


### Attacker model(s)

Looking at transient execution vulnerabilities, a relevant question is under which circumstances they can be exploited. Let us establish some terminology first:
- There is a *secret* which the *attacker* wants to leak, for instance the root password hash stored in /etc/shadow.
- The secret is loaded into memory by a *victim process*, for instance by an ssh server.
- The attacker can execute an *exploit* that implements a side-channel attack.

In general, the exploit must be executed on the same CPU core or thread as the victim process, and in most cases the exploit must also be able to control when the victim process loads the secret, for instance by attempting to login to the ssh server.


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

There are two typical ways to exploit this vulnerability:

1. _Flush and Reload._ The attacker flushes the CPU cache to ensure that her
   data is not in the cache, runs the code that depends on the sensitive data
   inside the kernel, and then measures that access time to her data to see if
   the sensitive code has accessed her data. For example, the code could look
   like this:

   ```c
   int local[10];

   int main(){
     // …
     
     // ← flush the local from CPU cache here
     
     // some pointer to kernel memory that we want to read
     int *ptr = …;
     
     // This like will cause SIGSEGV signal, because the CPU does not allow
     // us to read the *ptr. The SIGSEGV will cause the interrupt handler to
     // run. For us it will look as if this line did not run, but the CPU will
     // actually run it and mess with the CPU cache states.
     int a = local[*ptr];
   }
   
   void interrupt_handler(){
     // Measure access time to each element of local to determine which element
     // of local is in the cache. If, say, local[5] is in the cache, then we
     // know that *ptr == 5.
   }
   ```

2. Prime and Probe. This one works similarty, but now instead of flushing we
   fill the cache with our data, and then run the sensitive code to see if it
   will displace our data from cache.

   For this type of attack, one must mind the
   [cache associativity](https://www.sciencedirect.com/topics/computer-science/set-associative-cache).
   CPUs usually don't allow any cache line from the memory to be stored in any
   cache block of the CPU, but each cache line has a set of CPU cache blocks
   where it can be stored (for one- and two-way associative caches these sets
   have size 1 and 2 respectively). Knowing the cache associativity of the CPU
   lets the attacker predict which memory accesses of the sensitive code can
   displace which parts of her own data from the cache.

Modern operating systems mitigate the Meltdown attack
by [not keeping the kernel memory mapped in the user
processes](https://en.wikipedia.org/wiki/Kernel_page-table_isolation). This
introduces a considerable performance overhead for syscalls. When the kernel
memory is not mapped, the speculative execution will not load anything into the
cache, and the attack will not work.

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

In this section, we discuss a new class of transient execution vulnerabilities which were named "microarchitectural data sampling (MDS)" by Intel. Different variations of vulnerabilities in this class have been named "Zombieload" or "rogue in-flight data load (RIDL)" by different groups of researchers who discovered them.

Unlike spectre and meltdown, MDS vulnerabilities do not use specific memory addresses to leak data, but instead use clever techniques to leak data that is being held in various Intel CPU internal buffers. This means that the attacker has limited control over which data is leaked and therefore needs to do additional filtering of the leaked data to find the information they are after. On the other hand, it is harder to mitigate these vulnerabilities.

Since the buffers and related speculative behavior are unique to Intel architectures, these vulnerabilities do not seem to work directly on other microarchitectures.


### Microarchitectural Fill Buffer Data Sampling (MFBDS) / Zombieload / RIDL

As described in the introduction above, on Intel architectures the line fill buffer (LFB) is, among other things, used to temporarily store memory addresses that were not found in cache and are therefore being fetched from memory. This increases performance because several addresses can be requested to be fetched from memory at the same time without having to wait for the result. In some cases, data may already be available in the LFB, and the CPU will speculatively load the data and continue to execute, even though the data may be completely unrelated to the requested data. As you might expect after reading about specter and meltdown, this can be exploited by a clever attacker.


### Microarchitectural Load Port Data Sampling (MLPDS) / RIDL

### Microarchitectural Store Buffer Data Sampling (MSBDS) / Fallout

### Microarchitectural Data Sampling Uncacheable memory (MDSUM)

### Transactional Asynchronous Abort (TAA) / Zombieload v2 / RIDL

### L1D Eviction Sampling (L1DES) / RIDL / CacheOut

### Vector Register Sampling (VRS) / RIDL

### Special Register Buffer Data Sampling (SRBDS) / CROSSTalk


## Load-Value Injection (LVI)

