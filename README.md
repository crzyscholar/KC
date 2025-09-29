# KC
`kc.c` and `Makefile` are for building the module out of tree.

## /patches 
This directory contains the patch files generated from my work on Linus's tree.
Applying these patches will add my module in `samples/hello_world/` directory and make
according changes to relevant Kconfig and Makefile files so the module appear as a new config option
and can be built in-tree.

These patches were created on top of the master branch at commit 51a24b7deaae5c3561965f5b4b27bb9d686add1c.



