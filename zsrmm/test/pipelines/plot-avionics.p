set tics out
#set xtics format " "
#set ytics format " "
unset xtics
unset ytics
set yrange [0.8:1.1]
set xrange [0:11000]
set terminal pdf
set output 'pipeline-plot.pdf'
set tmargin 0
set bmargin 0
set multiplot layout 5,1 title "Avionics Example" font ",6"
set ylabel "CPU 0"
plot './ts-airspeed.txt' notitle w i , './ts-gps-position.txt' notitle w i, './ts-air-radar.txt' notitle w i, './ts-ground-radar.txt' notitle w i
set ylabel "CPU 1"
plot   './ts-lift.txt' notitle w i, './ts-stop-distance.txt' notitle w i , './ts-object-identification.txt' notitle w i, './ts-terrain-distance.txt' notitle w i
set ylabel "CPU 2"
plot  './ts-stall.txt' notitle w i, './ts-stop-location.txt' notitle w i , './ts-track-building.txt' notitle w i , './ts-time-to-terrain.txt' notitle w i
set ylabel "CPU 3"
set xtics scale 0 font ",6"
plot  './ts-angle.txt' notitle w i, './ts-virtual-runway.txt' notitle w i, './ts-traffic-warning.txt' notitle w i , './ts-terrain-warning.txt' notitle w i 
unset multiplot
