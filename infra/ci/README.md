# CI

Owner: security-devops-engineer. The workflow lives at
`.github/workflows/ci.yml` (GitHub requires that path); this directory holds
any future pipeline scripts/config it calls. Runs on every push/PR: install,
build all workspaces, run qa-engineer's test suites. An Android job is added
in Phase 1 when `apps/android` has a Gradle project.
