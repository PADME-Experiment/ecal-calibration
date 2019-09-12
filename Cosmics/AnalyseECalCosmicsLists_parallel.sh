#!/bin/bash

############################
# This script runs the OnlyCosmicsECal.cpp code on raw data to create trees
# with info about cosmic.
# The scrip also:
# - sources the configuration file configure.sh
# - creates (if needed) the subdirectory cosmics/ where analysed data are saved
# - creates the analysed list file(s) in the sub-directory
# - asks what to do with already exisiting list files of analysed data 
#   (it does NOT check their content, only the list file name):
#     >skip (keep the old version)
#     >overwrite
#     >append (you may append a file already present in the list)
# - can take in input a command to be applied to all the already existing list
#   file of analysed data:
#     > -s (skip)
#     > -o (overwrite)
#     > -a (append)
# - manages non existing input list files, non existing raw (root) files and 
#   empty lines in input list files
#
# With respect to AnalyseCosmicsLists.sh this script uses parallel to speed up
# the analysis. To do this it creates the file CommandFile.txt, so DON'T DELETE
# OR MODIFY IT DURING THE ANALYSIS. It will be deleted automatically at the end 
# of the process.
#
# Run as (you can analyse all the list files you want, here with the overwrite
# option):
# ./AnalyseECalCosmicsList_parallel.sh test1.list test2.list test3.list -o
############################

# Color definition
RED='\033[0;31m'
GREEN='\033[0;32m'
NC='\033[0m' # No Color


# To configure the system if not already done
echo -e "\nsource configure.sh\n"
source configure.sh



# if not present, create directory cosmics
outpath=$(pwd)/../../cosmics/output

if [ ! -d "$outpath" ]
then
    echo -e  "\n${GREEN}Creating directory $(pwd)/../../cosmics/output${NC}\n"
    $(mkdir $(pwd)/../../cosmics)
    $(mkdir $(pwd)/../../cosmics/output)
fi


opt="0"
for arg in "$@";do
    if [ "$arg" = "-s" ]
    then
	opt="s"
    fi
    if [ "$arg" = "-o" ]
    then
	opt="o"
    fi
    if [ "$arg" = "-a" ]
    then
	opt="a"
    fi
done

CommandFile="$(pwd)/CommandFile.txt"
if [ -f "$CommandFile" ]
then
    echo -e "\nRemoving old $CommandFile\n"
    $(rm $CommandFile)
fi
echo -e "\n${RED}DO NOT REMOVE THE FILE (commands for parallel) $CommandFile${NC}\n"

# Loop over list files in input
for RUN in "$@"; do
    echo -e "\n\n=<>=<>=<>=<>=<>=<>=<>=<>=<>=<>=<>=<>=<>=<>=<>=<>=<>=<>=<>=<>=<>="
    echo "=<>=<>=<>=<>=<>=<>=<>=<>=<>=<>=<>=<>=<>=<>=<>=<>=<>=<>=<>=<>=<>="

    # Check that the list file exists
    if [ -f "$RUN" ]
    then
        # output file list creation
	inlist=$(basename "$RUN")
	outlist=$outpath/CR_$inlist
	echo -e "\n${GREEN}Writing in: $outlist${NC}\n"
	
        # Check if the output list file is already existing
	if [ -f $outlist ]
	then
	    ans=$opt
	    while [ "$ans" != "s" ] && [ "$ans" != "o" ] && [ "$ans" != "a" ]; do
		echo -e "\n${RED}$outlist already exists."
		echo -e "Skip, Overwrite or Append (WARNING: will NOT check if a file is already present in the list)? [s/o/a]${NC}"
		read ans
		echo -e "\n"
	    done # end of while loop over ans
	    
	    if [ "$ans" = "s" ]
	    then
	        # Skip this run
		echo "Skipping this run: old list file will be kept."
	    fi
	    
	    if [ "$ans" = "o" ]
	    then
		echo -e "Deleting $outlist to re-write it.\n\n"
		$(rm $outlist)
	    fi
	fi # end of if that checks the output list existence
	
        # Perform analysis and write the output file list
	if [ ! -f $outlist ] || [ -f $outlist ] && [ "$ans" != "s" ]
	then
	    counter=0
	    while IFS='' read -r line || [[ -n "$line" ]]; do
		counter=$((counter+1))
		echo -e "\n\n=================================================="
		#echo "Opening: $line"
		infile=$(basename "$line" | cut -d. -f1)
		if [ ! "$infile" = "" ]
		then
		    if gfal-ls "$line"
		    then
			outfile=NTU_$infile
			#echo $line $outfile
			echo -e "\n"
			echo "File $line will be analysed."
			echo "echo;echo ====================================;echo;./OnlyCosmicsECal.exe -i $line -o $outfile ; ls $outpath/$outfile.root >> $outlist" >> $CommandFile
		    else
			echo -e "\n${RED}File $line does not exist. Moving to next file.${NC}"
		    fi
		    
		else
		    echo -e "${RED}\nLine no. $counter in file $RUN is empty (an unwanted newline command?)${NC}"
		fi # end of if-else on line inside list file	
		
	    done < "$RUN" # end of wile loop over root file in RUN list file
	    
	fi # end of file analysis

    else
	if [ "$RUN" != "-s" ] && [ "$RUN" != "-o" ] && [ "$RUN" != "-a" ]
	then	
	    echo -e "\n${RED}List file $RUN does not exists.${NC}\n"
	fi

    fi # end of check of RUN list file existence
	    
done # end of loop on RUN list files

echo ""


#############
# Parallel
if [ -f "$CommandFile" ]
then
    echo -e "\n\n\t########################################\n\n\t# START OF THE ANALYSIS USING PARALLEL #\n\n\t########################################\n\n\n"
    parallel -j 10 < $CommandFile
    $(rm $CommandFile)
echo -e "\n\n\n========================================"
    echo "rm $CommandFile"
fi