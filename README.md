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

#### Out-of-order and Speculative Execution

As programmers, we think of CPU instructions as being executed sequentially, one by one, but in practice instructions can be executed out-of-order. This happens when one or more previous instructions are waiting to complete, and the following instructions have no data dependency on the previous instructions. Simply put, if instructions don't need to know any of the resulting values of the previous instructions, they are ready to execute and the CPU will put them in a pipeline.

Note that we specifically qualify the instruction dependency to data dependencies, because control flow dependencies might be ignored if speculative execution is allowed. TODO: explain SE here.

Example sentence^[example reference here] here.


#### Branch Prediction

#### Hyperthreads & CPU internal buffers


### Attacker model(s)


### Relevant Cache Side Channel Attacks

#### Flush+Reload

#### Evict+Reload

#### ...


## Spectre & Meltdown Variants

### Spectre v1 - Bounds Check Bypass (BCB)

### Spectre v2 - Branch Target Injection (BTI)

### Meltdown v3 - Rogue Data Cache Load (RDCL)

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

