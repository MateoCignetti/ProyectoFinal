# Migration Checklist

Use this checklist to track your progress migrating to the component-based architecture.

## Phase 1: Preparation ✓ (Done)
- [x] Create new directory structure
- [x] Create CMakeLists.txt for each module
- [x] Create public interface headers
- [x] Create migration script
- [x] Create documentation

## Phase 2: File Migration

### Dimmer Module
- [ ] Run migration script: `./migrate_to_components.sh`
- [ ] Verify files copied to `components/dimmer_module/`
  - [ ] include/dimmer_interface.h
  - [ ] include/dimmer_control.h
  - [ ] src/dimmer_interface.c
  - [ ] src/dimmer_control.c
  - [ ] ui/*.c and ui/*.h files

### Buck Module
- [ ] Verify files copied to `components/buck_module/`
  - [ ] include/buck_interface.h
  - [ ] include/buck_control.h
  - [ ] src/buck_interface.c
  - [ ] src/buck_control.c
  - [ ] ui/*.c and ui/*.h files

## Phase 3: Code Updates

### Update Main CMakeLists.txt
- [ ] Open `CMakeLists.txt` in project root
- [ ] Change `EXTRA_COMPONENT_DIRS` from:
  ```cmake
  set(EXTRA_COMPONENT_DIRS "modules_src" "modules_src/ui_buck" "modules_src/ui_dimmer" "src")
  ```
  To:
  ```cmake
  set(EXTRA_COMPONENT_DIRS "components/dimmer_module" "components/buck_module" "src")
  ```

### Fix Include Paths (if needed)
- [ ] Check dimmer_interface.c includes
- [ ] Check buck_interface.c includes
- [ ] Check UI file includes

### Update module_manager.c
- [ ] Add includes:
  ```c
  #include "dimmer_module.h"
  #include "buck_module.h"
  ```
- [ ] Replace old function calls with new module API

## Phase 4: Build & Test

### First Build Attempt
- [ ] Run: `idf.py build`
- [ ] Fix any compilation errors
- [ ] Document any issues encountered

### Symbol Conflict Resolution (if needed)
- [ ] If you see duplicate symbol errors, run:
  ```bash
  python3 rename_dimmer_symbols.py
  ```
- [ ] Rebuild: `idf.py build`

### Test Build
- [ ] Build succeeds without errors
- [ ] No linker errors
- [ ] Binary size is reasonable

## Phase 5: Testing

### Flash and Test
- [ ] Flash to device: `idf.py flash`
- [ ] Monitor output: `idf.py monitor`
- [ ] Test buck module detection
- [ ] Test dimmer module detection
- [ ] Test UI functionality
- [ ] Test module switching

## Phase 6: Cleanup

### After Successful Testing
- [ ] Backup old structure (optional):
  ```bash
  mkdir backup
  mv modules_src/ui_buck backup/
  mv modules_src/ui_dimmer backup/
  mv modules_include/ui_buck backup/
  mv modules_include/ui_dimmer backup/
  ```
- [ ] Remove old UI directories:
  ```bash
  rm -rf modules_src/ui_buck
  rm -rf modules_src/ui_dimmer
  rm -rf modules_include/ui_buck
  rm -rf modules_include/ui_dimmer
  ```
- [ ] Keep modules_src for now (contains module interfaces)
- [ ] Update .gitignore if needed

## Phase 7: Documentation

- [ ] Update ARCHITECTURE.md with new structure
- [ ] Update README.md with new build instructions
- [ ] Document any changes made during migration
- [ ] Add notes about the new component structure

## Notes & Issues

Use this space to track any problems or decisions made during migration:

### Issues Encountered:
-

### Solutions Applied:
-

### Decisions Made:
-

---

## Quick Reference Commands

```bash
# Make migration script executable
chmod +x migrate_to_components.sh

# Run migration
./migrate_to_components.sh

# Clean build
idf.py fullclean

# Build
idf.py build

# Flash and monitor
idf.py flash monitor

# Rename dimmer symbols (if needed)
python3 rename_dimmer_symbols.py
```
