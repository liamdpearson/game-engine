# Qt Editor Roadmap

Plan for learning Qt and building a scene editor for this engine.

## Framing

Qt is the easy half. The engine currently has no scene *data* — the scene is built as
C++ stack variables in `src/main.cpp` and pushed into a global `rootObjs`. An editor
edits data, so most of the work is engine refactoring. Qt learning can happen in parallel.

Considered alternative: **Dear ImGui** would get a working editor in roughly a tenth the
effort, since it draws inside the existing GL loop. Qt was chosen deliberately — it is the
toolkit behind Maya, Houdini, and Substance, and it provides dockable panels, undo/redo,
and native file dialogs for free.

---

## Progress

| Item | Status |
|---|---|
| Phase 0.1 — CMake conversion | **Done** |
| Phase 0.1 — `main.h` globals removed | **Done** (file deleted; globals now live in `main.cpp`) |
| Phase 0.2 — `unique_ptr` ownership | **Done** — factories return `unique_ptr`, `rootObjs`/`children` converted, engine runs |
| Phase 0.2 — scene serialization | **Not started — resume here** |
| Phase 0.3 — decouple GLFW | Not started |
| Phase 1+ — Qt | Not started |

---

## Phase 0 — Engine prerequisites (no Qt involved)

### 0.1 Move to CMake — DONE

The handwritten `makefile` could not run Qt's `moc` (the meta-object compiler that
generates signal/slot code); Qt effectively requires CMake or qmake.

Three targets now exist:

```
third_party  (static lib: glad, xatlas, ufbx — built without -Wall -Wextra)
engine       (static lib: src/graphics, lighting, load, collisions, ui)
app          (exe: src/main.cpp, links engine)
```

`third_party` and `glfw3` are linked `PUBLIC` on `engine`, so anything linking `engine` —
including the future `editor` — inherits the include dirs and libraries automatically.
That is the whole reason for the split.

Build commands:

```
cmake --build build                          # everyday build
cmake --build build --target clean
cmake -S . -B build -G "MinGW Makefiles"     # only after editing CMakeLists.txt
```

`RUNTIME_OUTPUT_DIRECTORY` pins `app.exe` to the repo root on purpose — asset paths like
`"assets/testing_zone/testing_zone.obj"` resolve against the working directory.

A `build-debug/` directory configured with `-DCMAKE_BUILD_TYPE=Debug` is useful for
getting real backtraces out of gdb. Note it also writes to the repo root, so rebuild the
release target afterwards to restore the optimized `app.exe`.

### 0.2 Make the scene serializable

The actual gate. Requirements:

- **Stable ownership** — `std::vector<std::unique_ptr<Object>>` instead of raw pointers
  to stack locals. *(Done.)*
- **A `name` and stable `id` on `Object`.** *(Todo.)*
- **A type tag** so the loader knows whether to build a `StaticMesh`, `AnimatedObj`,
  `Capsule`, or `Camera`. *(Todo.)*
- **`saveScene(path)` / `loadScene(path)`** — JSON is fine. Write the loader in the engine
  so `app` does not depend on Qt; the editor side can use `QJsonDocument`. *(Todo.)*

**Success criterion:** delete the hardcoded scene from `main.cpp`, replace it with
`loadScene("assets/test.scene")`, and the game runs identically.

Sketch of the loader shape:

```cpp
std::unique_ptr<Object> obj;                    // null on purpose
if      (type == "static")   obj = makeStaticMesh(...);
else if (type == "animated") obj = makeAnimatedObj(...);
else                         obj = std::make_unique<Object>();

rootObjs.push_back(std::move(obj));
```

### 0.3 Decouple rendering from GLFW

`graphics.cpp` owns a global `GLFWwindow* window` and `SW`/`SH`, and `lighting.cpp:340`
calls `glfwGetTime()`. For the viewport to live inside a Qt widget the engine must not
create its own window.

- Pass viewport dimensions into the draw call instead of reading globals.
- Get time from `std::chrono` rather than `glfwGetTime()`.
- Route input through a small struct the host (GLFW *or* Qt) fills in.

---

## Phase 1 — Qt setup

- Install **Qt 6** via the online installer. Pick the **MinGW kit**, not MSVC —
  `third_party/glfw/lib/libglfw3.a` is a MinGW archive, and Qt binaries are ABI
  incompatible across compilers. Use Qt's bundled MinGW g++ to build *everything*.
- Install **Qt Creator** for learning — its designer, docs integration, and debugger are
  the fastest way in, even if VS Code is used later.
- Use **Qt Widgets, not QML.** QML targets touch/animated UIs; serious desktop tools are Widgets.

Current local toolchain is TDM-GCC 10.3.0. Qt 6 ships MinGW 13.x, so configure a
**separate build directory** for the Qt toolchain (`cmake -S . -B build-qt`) rather than
reusing `build/` — objects from two compilers must never mix. GLFW is pure C so
`libglfw3.a` will most likely still link.

---

## Phase 2 — Learn Qt in the order the editor needs it

Do not work through a general Qt tutorial front to back. Learn these five, each with a
throwaway toy app:

1. **Object tree & signals/slots** — `QObject` parent ownership (parents delete children),
   `connect()` with new-style function-pointer syntax.
2. **`QMainWindow` + `QDockWidget`** — the editor skeleton: menu bar, central viewport,
   dockable hierarchy and inspector panels.
3. **Model/View** — `QTreeView` plus a custom `QAbstractItemModel` wrapping the `Object`
   hierarchy. Hardest Qt concept; budget real time.
4. **`QUndoStack` / `QUndoCommand`** — do this **early**, not as a retrofit. If every edit
   is a `QUndoCommand` from day one, undo/redo is free forever.
5. **`QOpenGLWidget`** — hosting the renderer.

Resources: the official `doc.qt.io` Qt Widgets examples are the canonical reference. KDAB
publishes free talks on Qt internals and model/view. Skip Qt4-era tutorials — the
signal/slot syntax changed.

---

## Phase 3 — Editor milestones

| # | Milestone | Teaches |
|---|---|---|
| 1 | Qt app opens the `.scene` JSON and shows objects in a read-only `QTreeView` | model/view |
| 2 | Inspector panel: click an object, get spinboxes for its `Transform` | widgets, signals |
| 3 | Edit + save; `app.exe` launched separately picks up the change | round-trip |
| 4 | Everything routed through `QUndoCommand`, `Ctrl+Z` works | undo architecture |
| 5 | `QOpenGLWidget` viewport rendering the scene live | GL embedding |
| 6 | Editor camera (WASD/orbit), click-to-select via ray pick | reuses `collisions.cpp` |
| 7 | Translate/rotate gizmo, add/delete objects, asset browser | the real editor |

Milestone 3 is already a genuinely usable tool. Do not skip ahead to the viewport.

---

## Two OpenGL gotchas that will each cost an afternoon

**`QOpenGLWidget` does not render to framebuffer 0.** It renders into an FBO it owns. Any
engine code doing `glBindFramebuffer(GL_FRAMEBUFFER, 0)` to "return to the screen" draws
into the void. Use `QOpenGLWidget::defaultFramebufferObject()` — make the engine take the
default FBO id as a parameter rather than hardcoding `0`.

**glad vs. Qt's GL headers conflict.** Qt pulls its own `GL/gl.h` in through `qopengl.h`,
which collides with glad. Include `<glad/glad.h>` *before* any Qt header in every
translation unit touching GL, and initialize with
`gladLoadGLLoader((GLADloadproc)QOpenGLContext::currentContext()->getProcAddress)` inside
`initializeGL()`. Do not mix in `QOpenGLFunctions` — pick one loader.

---

## Appendix — C++ lessons from the ownership refactor

Notes worth keeping, since the same traps recur.

**Raw pointer vs `unique_ptr`.** A raw `Object*` is just an address and says nothing about
who destroys the object. A `unique_ptr<Object>` *is* the owner. Same size, zero runtime
overhead — it is purely a compile-time statement about ownership.

**Owning links get `unique_ptr`, observing links stay raw.**

```cpp
std::vector<std::unique_ptr<Object>> children;  // parent OWNS its children
Object* parent = nullptr;                       // child only OBSERVES its parent
```

`parent` must stay raw — if both directions owned, the reference cycle would leak.

**Never store scene objects by value in a vector.** `std::vector<Object>` reallocates on
growth and moves every object to a new address, instantly dangling every `parent` pointer
and every `children` entry. `unique_ptr` keeps objects at fixed heap addresses no matter
how the vector grows. This is what makes runtime add/delete in the editor safe.

**Declaring a destructor suppresses the implicit move constructor.** `Object`, `Mesh`, and
`StaticMesh` all declare destructors, so returning them by value fell back to *copy* —
which became ill-formed once `children` held `unique_ptr`. This produced the
`static assertion failed: result type must be constructible from value type of input range`
error. Fix was returning `unique_ptr` from the factories, not adding move constructors:
`~Mesh()` calls `glDeleteVertexArrays`/`glDeleteBuffers`/`glDeleteTextures`, so a defaulted
move would copy the GL handles and let the moved-from destructor delete buffers still in use.

**Every read of a `unique_ptr` must come before the `std::move` that hands it off.**
This bit twice — once in `main.cpp` (calling `.get()` after `push_back(std::move(...))`,
yielding null raw pointers) and once inside all three `addChild` overloads:

```cpp
// WRONG — correct with raw pointers, segfaults with unique_ptr
this->children.push_back(std::move(child));
child->parent = this;                       // child is null here

// RIGHT
child->parent = this;
this->children.push_back(std::move(child));
```

`std::move` does not move anything by itself — it is a cast granting `push_back` permission
to gut the source. The emptying happens inside `push_back`, so the variable is dead the
instant that call returns.

**`std::unique_ptr<T> obj;` creates nothing** — it is a null handle, equivalent to
`T* obj = nullptr;`. `std::make_unique<T>()` is what allocates. Prefer it over
`unique_ptr<T>(new T)`: the type name appears once, no raw `new` to audit, and no
exception-safety gap in argument lists. Use `auto` when the type is already spelled on the
right; spell it out when deliberately taking a base handle
(`std::unique_ptr<Object> obj = std::make_unique<StaticMesh>();` — legal because `Object`
has a virtual destructor).

**Debugging tip.** For segfaults, configure a debug build and get a real backtrace instead
of guessing:

```
cmake -S . -B build-debug -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Debug
cmake --build build-debug
gdb -batch -ex run -ex bt ./app.exe
```
