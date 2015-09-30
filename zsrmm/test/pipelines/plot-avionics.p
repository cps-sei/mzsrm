unset ytics
#unset xtics
set xtics out
set xtics rotate font ",6" 0, 500, 11000
set xtics format ''
set mxtics 5
set yrange [0.8:1.1]
set xrange [0:11000]
set terminal pdf
set output 'pipeline-plot.pdf'
set tmargin 0
set bmargin 0
set key font ",5"
set multiplot layout 5,1 title "Avionics Example" font ",6"
set ylabel "CPU 0"
set key horiz
set key center top
plot './ts-airspeed.txt' title "airspeed" w i , './ts-gps-position.txt' title "gps" w i, './ts-air-radar.txt' title "a-radar" w i, './ts-ground-radar.txt' title "g-radar" w i
set ylabel "CPU 1"
set key horiz
set key center top
plot   './ts-lift.txt' title "lift" w i, './ts-stop-distance.txt' title "stop-dist" w i , './ts-object-identification.txt' title "object" w i, './ts-terrain-distance.txt' title "terrain-d" w i
set ylabel "CPU 2"
set key horiz
set key center top
plot  './ts-stall.txt' title "stall" w i, './ts-stop-location.txt' title "stop-loc" w i , './ts-track-building.txt' title "track" w i , './ts-time-to-terrain.txt' title "t-terrain" w i
set ylabel "CPU 3"
set xtics format "%g"
set key horiz
set key center top
plot  './ts-angle.txt' title "angle" w i, './ts-virtual-runway.txt' title "v-runway" w i, './ts-traffic-warning.txt' title "traffic-w" w i , './ts-terrain-warning.txt' title "terrain-w" w i 
unset multiplot
