# FreeRTOS task awareness with OpenOCD

## Finding

OpenOCD supports FreeRTOS task-aware debugging, but it is opt-in and depends on
symbols in the application ELF.  Its [RTOS documentation](https://openocd.org/doc/html/GDB-and-OpenOCD.html#RTOS-Support)
lists these nine required FreeRTOS symbols: `pxCurrentTCB`, `pxReadyTasksLists`,
`xDelayedTaskList1`, `xDelayedTaskList2`, `pxDelayedTaskList`,
`pxOverflowDelayedTaskList`, `xPendingReadyList`, `uxCurrentNumberOfTasks`,
and `uxTopUsedPriority`.  OpenOCD also says that some FreeRTOS versions may require its
[`contrib/rtos-helpers/FreeRTOS-openocd.c`](https://github.com/openocd-org/openocd/blob/master/contrib/rtos-helpers/FreeRTOS-openocd.c)
helper to be linked.

The current debug ELF exports only `pxCurrentTCB` globally.  The remaining
required scheduler objects are local symbols: FreeRTOS declares them through
its `STATIC` macro unless `portREMOVE_STATIC_QUALIFIER` is defined.  This is
intentional upstream behavior: the
[kernel source](https://github.com/FreeRTOS/FreeRTOS-Kernel/blob/main/tasks.c)
states that some kernel-aware debuggers require these data objects to be global
and defines `STATIC` as `static` unless that macro is set.

Therefore the observed `No symbols for FreeRTOS` message is a compatibility
failure in the task-awareness layer, not an SWD/ST-LINK transport failure and
not evidence that FreeRTOS itself is broken.  In OpenOCD's
[FreeRTOS backend](https://openocd.org/doc-release/doxygen/FreeRTOS_8c_source.html),
that exact message is emitted when no RTOS symbol table has been supplied; the
backend cannot enumerate tasks afterward.

## Recommended configuration

For normal source-level debugging, keep RTOS awareness disabled.  This avoids
the extra GDB/OpenOCD symbol negotiation and target-memory reads used to build
the task list.  The current launch configuration should retain only the STM32
target, SWD probe speed, executable, and `runToEntryPoint` settings; do not add
an RTOS option merely because the firmware uses FreeRTOS.

Enable task-aware debugging only when selecting FreeRTOS tasks in the debugger
is worth the extra stop-time work.  Make it an explicit, separately named
launch profile, and prepare the firmware as follows:

1. Compile the FreeRTOS kernel with `portREMOVE_STATIC_QUALIFIER` so the
   scheduler objects required by OpenOCD have external linkage.
2. For FreeRTOS versions that do not provide `uxTopUsedPriority`, add OpenOCD's
   upstream `FreeRTOS-openocd.c` helper. FreeRTOS V11.3.1 already defines this
   symbol, so adding the helper here would cause a duplicate definition.
3. When section garbage collection is enabled, retain every required symbol at
   final link; the helper's upstream comment specifically calls out
   `--undefined=uxTopUsedPriority` and the other required symbols.
4. Verify the resulting ELF with `arm-none-eabi-nm -g --defined-only` before
   enabling the OpenOCD target's `-rtos FreeRTOS` option.

The OpenOCD manual documents that RTOS support is disabled by default, enabled
through the target's `-rtos` option, and can be disabled again with `-rtos none`.
This makes a plain, non-RTOS launch profile the right performance baseline.

## Validation

After an RTOS-aware build, confirm that all nine required names appear in the
global defined-symbol table, launch with `-rtos FreeRTOS`, and stop after the
scheduler has started.  OpenOCD should then expose the FreeRTOS task list to
GDB without `No symbols for FreeRTOS`.  Measure stop/step latency with the
plain and RTOS-aware profiles separately; task enumeration necessarily performs
additional target-memory reads (as visible in the backend source), so it should
not be enabled for a speed-focused profile unless task selection is needed.
