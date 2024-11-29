0-256: 8
256-512: 8


what do we want out of memory tracing?
- leak detection
- alloc stats: srcloc, size, count, scopes
- tracing / simulation

we don't plan on enabling the memory tracer in shipping builds, so we don't care how much extra memory it uses