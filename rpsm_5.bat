start /b RPSM.exe -host_sid 0 -rpspgm_name Ta_RPS_Flight800 > RPSmExe0.log
start /b RPSM.exe -host_sid 1 -rpspgm_name Ta_RPS_Flight825 > RPSmExe1.log
start /b RPSM.exe -host_sid 2 -rpspgm_name Ta_RPS_Stupid > RPSmExe2.log
start /b RPSM.exe -host_sid 3 -rpspgm_name Ta_RPS_Random > RPSmExe3.log
RPSM.exe -host_sid 4 -rpspgm_name Ta_RPS_Flight800 > RPSmExe4.log
