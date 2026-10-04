.macro jlabel label
    .global \label
    \label:
.endm

.macro glabel label
    \label:
.endm

.macro .def #
.endm

.include "gte.inc"
