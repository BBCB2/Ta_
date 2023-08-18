start /b RPS.exe -trace_level 0 -host_sid 0 -rpspgm_name Ta_RPS_Flight800 > RPSuExe0.log
start /b RPS.exe -trace_level 0 -host_sid 1 -rpspgm_name Ta_RPS_Flight825 > RPSuExe1.log
start /b RPS.exe -trace_level 0 -host_sid 2 -rpspgm_name Ta_RPS_Stupid > RPSuExe2.log
start /b RPS.exe -trace_level 0 -host_sid 3 -rpspgm_name Ta_RPS_Random > RPSuExe3.log
RPS.exe -trace_level 0 -host_sid 4 -rpspgm_name Ta_RPS_Flight800 > RPSuExe4.log
