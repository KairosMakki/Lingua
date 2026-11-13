# ELF Format

Parsed: ELF header (32/64-bit, both endiannesses), section headers, program headers, dynamic section (DT_NEEDED, RPATH/RUNPATH), security hardening flags (PIE, RELRO, NX, stack canary).

Not parsed: symbols, relocations (partial).