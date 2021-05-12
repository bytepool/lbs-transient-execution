# A Case Study of Transient Execution Vulnerabilities

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

- Line Fill buffer.
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

## Known transient execution vulnerabilities

In this section, we briefly outline the different types of transient execution vulnerabilities that have been found so far, before we dive deeper into a small selection of them later.

### Spectre & Meltdown Variants

TODO: short high level description of Spectre & Meltdown here.

- Spectre v1 - Bounds Check Bypass (BCB)
- Spectre v2 - Branch Target Injection (BTI)
- Meltdown - Rogue Data Cache Load (RDCL)
- Spectre-NG v3a - Rogue System Register Read (RSRR)
- Spectre-NG - Lazy FP State Restore
- Spectre-NG v1.1 - Bounds Check Bypass Store (BCBS)
- Spectre-NG v4 - Speculative Store Bypass (SSB)
- SpectreRSB - Return Mispredict
- Spectre SWAPGS

### Foreshadow - L1 Terminal Fault

TODO: short high level description of L1TF here.

The following variants of this vulnerability have been assigned a CVE:

- Foreshadow
- Foreshadow-OS
- Foreshadow-VMM


### Microarchitectural Data Sampling

In this section, we discuss a new class of transient execution vulnerabilities which were named "microarchitectural data sampling (MDS)" by Intel. Different variations of vulnerabilities in this class have been named "Zombieload" or "rogue in-flight data load (RIDL)" by different groups of researchers who discovered them.

Unlike spectre and meltdown, MDS vulnerabilities do not use specific memory addresses to leak data, but instead use clever techniques to leak data that is being held in various Intel CPU internal buffers. This means that the attacker has limited control over which data is leaked and therefore needs to do additional filtering of the leaked data to find the information they are after. On the other hand, it is harder to mitigate these vulnerabilities.

Since the buffers and related speculative behavior are unique to Intel architectures, these vulnerabilities do not seem to work directly on other microarchitectures.

The following variants of this vulnerability have been assigned a CVE:

- Microarchitectural Fill Buffer Data Sampling (MFBDS) / Zombieload / RIDL
- Microarchitectural Load Port Data Sampling (MLPDS) / RIDL
- Microarchitectural Store Buffer Data Sampling (MSBDS) / Fallout
- Microarchitectural Data Sampling Uncacheable memory (MDSUM)
- Transactional Asynchronous Abort (TAA) / Zombieload v2 / RIDL
- L1D Eviction Sampling (L1DES) / RIDL / CacheOut
- Vector Register Sampling (VRS) / RIDL
- Special Register Buffer Data Sampling (SRBDS) / CROSSTalk


### Load-Value Injection (LVI)


## Case Studies

In this section we discuss a subset of the different variants highlighted above in more detail. The chosen vulnerabilities are:

- Meltdown & Spectre (v1 - v3)
- Microarchitectural Fill Buffer Data Sampling (MFBDS) / Zombieload / RIDL

In addition, we adapt some known proof of concepts and apply them to new victim applications. We also look into possible mitigations.

### Meltdown & Spectre

#### Meltdown - Rogue Data Cache Load (RDCL)

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

Modern operating systems mitigate the Meltdown attack by [not keeping the
kernel memory mapped in the user
processes](https://en.wikipedia.org/wiki/Kernel_page-table_isolation). This
introduces a considerable performance overhead for syscalls. When the kernel
memory is not mapped, the speculative execution will not load anything into the
cache, and the attack will not work.

#### Spectre v1 - Bounds Check Bypass (BCB)

Similarly to Meltdown, Spectre v1 also relies on the CPU cache and speculative
execution. But in addition to that it also exploits the branch prediction
mechanism.

Imagine that the OS kernel (or another code running on a higher privilege
level) runs this piece of code (adapted from [Linaro
blog](https://www.linaro.org/blog/meltdown-spectre/)):

```c
if (x < array1_size)
    y = array2[array1[x]]
```

If all the values in `array2` are valid indices to address `array1`, this code
normally should be safe; even if the adversary can choose an arbitrary values
of `x` as an input.

When the `x` is not in CPU cache during the execution of the above snippet,
the comparison will take time to load the value into the cache. But this won't
stop the speculative execution, it will try to keep running the code. But what
code? The `if` body or the code after it? The CPU will ask its branch predictor
to decide, and the latter will answer based on how often the `if` body was
executed in the past.

So if the adversary is sure that the branch predictor will advice the CPU to
run the `if` body, she can pass such value for `x` that `array1 + x` address
points to memory she's interested in. And it will cause a memory access to
`array2 + *(array1 + x)`, which the attacker can detect using the standard
Meltdown techniques; both "Prime and Probe" and "Flush and Reload" can work.

This attack, if successfully implemented, can target pretty much any piece
of code that is running with higher privileges: OS kernel, virtualization
hypervisor, your programming language VM (Java, JS). But, in practice, this
attack is considered to be very difficult to implement: guessing what branch
predictor will do and looking for the right piece of code in the target program
both add a lot of complexity. The Spectre v2 improves this attack with a better
way to deal with branch predictor, making it a lot more practical.

#### Spectre v2 - Branch Target Injection (BTI)

The branch predictor keeps track of how many times each branch was taken. But,
of course, it would be impossible to have a counter inside CPU for each
location in RAM which can have a branch in it (modern computers can have tens
of gigabytes of memory; if you want a counter for each memory location, you
will have to have a few gigabytes of memory in your CPU branch predictor).

Therefore, branch predictor accumulates the statistics for multiple branches
in one counter. The mapping there is similar to the mapping of set associative
cache, no separation is enforced between different privilege levels.

It means, that an attacker can create some branches in her memory that will
share branch predictor counters with the target code, and then run those
branches multiple times to bias the counters (this is the "injection" part) in
the needed direction. This way, the attacker can ensure that the needed branch
will be taken by the target code from Spectre v1.

The possibility of this attack comes from the fact that branch predictor
counters are shared between different privilege levels. Modern processors
provide no way of isolating them or manually clearing the branch
predictor counters. While everyone is wating for CPU manufacturers
to design new processors will will have these issues addressed,
engineers from Google proposed a completely software mitigation called
[retpolines](https://support.google.com/faqs/answer/7625886) (return +
trampoline).

Retpoline technique shows how to replace any branching instruction by a few
other instructions which will behave the same and at the same time confuse
that branch predictor and not let it see that this is a branch. This fools the
speculative execution which thinks that some useless code (like infinite cycle)
is going to be executed next, while in reality control flow gets redirected
somewhere else.


### Microarchitectural Fill Buffer Data Sampling (MFBDS) / Zombieload / RIDL

In the following, we focus on the first variant which uses the line fill buffer (LFB) to leak data.

On Intel architectures the line fill buffer (LFB) is, among other things, used to temporarily store memory addresses that were not found in cache and are therefore being fetched from memory. This increases performance because several addresses can be requested to be fetched from memory at the same time without having to wait for the result. In some cases, data may already be available in the LFB, and the CPU will speculatively load the data and continue to execute, even though the data may be completely unrelated to the requested data. As you might expect after reading about specter and meltdown, this can be exploited by a clever attacker.


### MFBDS PoC: leaking the ssh host key
