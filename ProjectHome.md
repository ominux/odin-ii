# **NOTE** for the most up to date code base for OdinII, please go to: https://code.google.com/p/vtr-verilog-to-routing/ #

Odin II is a framework for Verilog Hardware Description Language (HDL) synthesis that allows researchers to investigate approaches/improvements to different phases of HDL elaboration that have not been previously possible. Odin II’s output can be fed into traditional back-end flows for both FPGAs and ASICs so that these improvements can
be better quantified. Whereas the original Odin provided an
open source synthesis tool, Odin II’s synthesis framework offers significant improvements such as a unified environment for both front-end parsing and netlist flattening. Odin II also interfaces directly with VPR, a common academic FPGA CAD flow, allowing an architectural description of a target FPGA as an input to enable identification and mapping of design features to custom features. Furthermore, Odin II can also read the netlists from downstream CAD stages into its netlist data-structure to facilitate analysis. Odin II can be used for a wide range of experiments; Odin II is open source and released under the MIT License.

Features:
  * Verilog Synthesis to BLIF
  * Synthesize functional blocks such as multiplier and adders to an FPGA flow
  * Visualization of the BLIF circuits
  * Simulate the generated BLIF with vectors

To reference this work in an academic paper please use:
"Odin II - An Open-source Verilog HDL Synthesis tool for CAD Research"
Peter Jamieson, Kenneth B. Kent, Farnaz Gharibian, and Lesley Shannon.
2010 Field-Programmable Custom Computing Machines (FCCM'10), 2010
Charolotte, North Carolina.

bibtex:

@inproceedings{odinII,
author  = {Peter Jamieson and Kenneth B. Kent and Farnaz Gharibian and Lesley Shannon},
title   = {{Odin II - An Open-source Verilog HDL Synthesis tool for CAD Research}},
booktitle = {{Proceedings of the IEEE Symposium on Field-Programmable Custom Computing Machines}},
year    = {2010},
pages = {149--156}}