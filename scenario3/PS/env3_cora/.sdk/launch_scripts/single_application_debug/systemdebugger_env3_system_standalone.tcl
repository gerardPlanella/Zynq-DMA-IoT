connect -url tcp:127.0.0.1:3121
targets -set -nocase -filter {name =~"APU*"}
rst -system
after 3000
targets -set -filter {jtag_cable_name =~ "Digilent Cora Z7 - 7007S 210370AD79CAA" && level==0} -index 1
fpga -file /home/gerard/Documents/TFG/ZynqDmaAnalysis/SW/env3_cora/env3/_ide/bitstream/env_1_wrapper.bit
targets -set -nocase -filter {name =~"APU*"}
loadhw -hw /home/gerard/Documents/TFG/ZynqDmaAnalysis/SW/env3_cora/env3_cora/export/env3_cora/hw/env_1_wrapper.xsa -mem-ranges [list {0x40000000 0xbfffffff}]
configparams force-mem-access 1
targets -set -nocase -filter {name =~"APU*"}
source /home/gerard/Documents/TFG/ZynqDmaAnalysis/SW/env3_cora/env3/_ide/psinit/ps7_init.tcl
ps7_init
ps7_post_config
targets -set -nocase -filter {name =~ "*A9*#0"}
dow /home/gerard/Documents/TFG/ZynqDmaAnalysis/SW/env3_cora/env3/Debug/env3.elf
configparams force-mem-access 0
targets -set -nocase -filter {name =~ "*A9*#0"}
con
