#!/usr/bin/bash

mkdir -p plot-data
cd plot-data

for log in "$@"
do
    tracker_version=${log#*-}
    tracker_version=${tracker_version%/*}

    dataset=${log#*/}
    dataset=${dataset#*-}
    dataset=${dataset%-*}

    mkdir -p "$dataset"

    cd "$dataset"

    arguments=${log%.*}
    arguments=${arguments#*/*-*-}

    mkdir -p "$arguments"

    cd "$arguments"

    data_file="$tracker_version".dat
    touch "$data_file"

    echo "${tracker_version^}" >> "$data_file"

    while IFS= read -r line 
    do
        #Skip last line
        if [[ $line != *"->"* ]]; then
           continue
        fi
        time=${line%,*,*}
        time=${time//[!.0-9]/}
        echo "$time" >> "$data_file"
    done < "../../../${log}"

    cd ../../
done

for directory in *
do
    cd "$directory"
    for subdirectory in *
    do
        cd "$subdirectory"
        paste *.dat > all
        gnuplot -persist << EOFMarker
        file = 'all'
        header = system('head -1 '.file)
        N = words(header)

        set title "${directory^}-${subdirectory}"
        set ylabel "Time [ms]"
        set xtics rotate
        set xtics ('' 1)
        set for [i=1:N] xtics add (word(header, i) i)

        set style data boxplot
        set style boxplot nooutliers
        unset key
        plot for [i=1:N] file using (i):i
EOFMarker
        cd ..
    done
    cd ..
done

cd ..
rm -r plot-data
