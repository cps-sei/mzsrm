set linetype 1 lc rgb 'black'
set linetype 2 lc rgb '#555555'
set linetype 3 lc rgb '#888888'
set linetype 4 lc rgb '#BBBBBB'
unset ytics
#unset xtics
#set xtics out
#set xtics rotate font ",10" 0, 1000, 16000
#set xtics format ''
set mxtics 5
set yrange [0.99:1.01]
#set yrange [0:1.1]
set xrange [0:16000]
set terminal pdf
set output 'pipeline-plot.pdf'
set tmargin 0
set bmargin 0
set key font ",5"
#set multiplot layout 5,1 title "Avionics Example" font ",6"
set multiplot layout 5,1 font ",6"
set size 1, 0.2
set xtics out
set xtics nomirror
#set xtics rotate font ",10" 0, 1000, 16000
set xtics font ",10" 0, 1000, 16000
set xtics format ''
set ylabel "CPU 0"
set key horiz
set key center top
#plot './ts-airspeed.txt' title "airspeed" w i , './ts-gps-position.txt' title "gps" w i, './ts-air-radar.txt' title "a-radar" w i, './ts-ground-radar.txt' title "g-radar" w i
plot './ts-airspeed.txt' notitle  w i , './ts-gps-position.txt' notitle  w i, './ts-air-radar.txt' notitle  w i, './ts-ground-radar.txt' notitle  w i
set ylabel "CPU 1"
set key horiz
set key center top
#plot   './ts-lift.txt' title "lift" w i, './ts-stop-distance.txt' title "stop-dist" w i , './ts-object-identification.txt' title "object" w i, './ts-terrain-distance.txt' title "terrain-d" w i
plot   './ts-lift.txt' notitle  w i, './ts-stop-distance.txt' notitle  w i , './ts-object-identification.txt' notitle  w i, './ts-terrain-distance.txt' notitle  w i
set ylabel "CPU 2"
set key horiz
set key center top
#plot  './ts-stall.txt' title "stall" w i, './ts-stop-location.txt' title "stop-loc" w i , './ts-track-building.txt' title "track" w i , './ts-time-to-terrain.txt' title "t-terrain" w i
plot  './ts-stall.txt' notitle w i, './ts-stop-location.txt' notitle  w i , './ts-track-building.txt' notitle  w i , './ts-time-to-terrain.txt' notitle  w i
set ylabel "CPU 3"
set xtics ("1" 1000,"2" 2000,"3" 3000,"4" 4000, "5" 5000, "6" 6000,"7" 7000, "8" 8000,"9" 9000,"10" 10000,"11" 11000,"12" 12000,"13" 13000, "14" 14000, "15" 15000,"16" 16000)
set xtics format "%g"
set key horiz
set key center top
#plot  './ts-angle.txt' title "angle" w i, './ts-virtual-runway.txt' title "v-runway" w i, './ts-traffic-warning.txt' title "traffic-w" w i , './ts-terrain-warning.txt' title "terrain-w" w i 
plot  './ts-angle.txt' notitle w i, './ts-virtual-runway.txt' notitle w i, './ts-traffic-warning.txt' notitle w i , './ts-terrain-warning.txt' notitle w i 
unset multiplot
