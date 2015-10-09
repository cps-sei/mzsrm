
set yrange [.5:3.5]
set xtics scale 0.5, 0.25 rotate 0,100,39000 font ",5"
set terminal pdf
set output 'pipeline-plot.pdf'
plot './ts-airspeed.txt' title "Airspeed" w i , './ts-lift.txt' title "Lift" w i, './ts-stall.txt' title "Stall" w i, './ts-angle.txt' title "Angle" w i
