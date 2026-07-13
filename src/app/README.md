# app — application shell & lifecycle

Entry point, command-line handling, program configuration, session/recent-files,
and wiring of the module singletons. Reuses SLADE's `Application` subsystem.
The `main()` here constructs the wxApp, initialises the Render Abstraction Layer
(`src/render`), and mounts the UI shell (`src/ui`).
