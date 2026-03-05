# Principles
Goal: minimize time/cost to understand and change the code. Everything else derives from this.
- Simple > Complex: Simple is easy to understand and change. Complex is not.
- Minimal > Excessive: No unnecessary things to understand or change.
- Transparent > Opaque: Transparent is easy to reason about end-to-end. Opaque creates firewalls in understanding.
- Obvious > Hidden: Our perceived understanding should match our actual understanding. 
- Explicit > Implicit: No guessing or needing insider information to understand something.
- Less > More: Code is a liability, not an asset. Less code = less to read and change.
- Fast > Slow: Code and product quality is proportional to the number of iterations we can make on the design. Fast = more iterations = better product.
- Readable > Writable: We write once, read hundreds of times. Easy to read/hard to write is okay. Easy to write/hard to read is unacceptable.
- Concrete > Abstract: Abstraction reduces perceived complexity but increases real complexity. Must only be used when the cost is *obviously* justified. This cost/justification is never known until the concrete/non-abstracted approach is tried and found wanting. We *never* abstract as a first approach.
- Specific > General: Solve only the known use-cases in the simplest possible way.
These principles should produce the smallest/simplest codebase for the current known use-cases, and NOTHING MORE.

# Rules
- For this project, we're trying to get away with including as few standard C/C++ headers as possible and do as much ourselves as possible. This is mainly as a learning exercise.
- Fix bad code immediately: coded that is hard to understand/change only gets worse over time and therefore more costly to change.
- Minimize code depth: Shallow code == fewer cognitive jumps needed to read a flow end-to-end.
- Arrays and Hash Tables are the only general data structures. Anything beyond this is a problem-specific. Contiguous memory structures with linear walkthroughs.
- Delete unused code, do not comment-out
- Minimize dependencies: A depends on B -> I can't understand A without understanding B, and I can't change to understand B.
- Minimize external libraries: we don't own the code, nobody understands how it works, and we can't easily change it.
- Solution complexity should be proportional to problem complexity: okay if complex shit is complex.
- In fact complex problems *MUST* be obviously complex in the code.
- Slow/expensive shit should be obviously slow/expensive in the code. Don't overload operator= to copy a database.
- Headers can only include "JC/Common.h", NO OTHER HEADERS. Forward declare everything else needed.
- Minimize code in header. I'm looking at you, C++ templates.
- Separate "orchestration"/"coordination" code from "worker" code. Keep the orchestration code in one place so it can be understood and reasoned about easily. Defer the "meat" to the worker modules.
- When abstracting, maximize the "volume:area" ratio: smallest possible interface, maximum possible functionality under the interfaces. This maximizes the value for the abstraction cost. 
- Separate runtime allocation into lifetime arenas. Anything one-frame or less goes on the temp arena which gets reset each frame.
- Track memory across all modules
- Make errors unignorable. Use the Err construct to pack as much context as possible into the error.
- Have clear memory ownership, never share across modules. Use handles to insulates API callers from internal memory details.
- Error telemetry matters: use Res<>/Err system to provide
- Defer decisions until as late as possible to provide maximum context before making the decision.
- Destructors, copy constructors, and operator= are banned. Everything is trivially copyable/POD except for a few C++ utilities.
- Dont fret about duplicated code until it becomes a real problem