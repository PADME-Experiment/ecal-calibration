#!/bin/bash

############################
# This script creates the file with the list of all the root (merged) 
# files of a certain run.
# The script also:
# - asks what to do with already exisiting list files
#   (it does NOT check their content, only the list file name):
#     >skip (keep the old version)
#     >overwrite
#     >append (you may append a file already present in the list)
# - can take in input a command to be applied to all the already existing list
#   file of analysed data:
#     > -s (skip)
#     > -o (overwrite)
#     > -a (append)
#
# If you are running on data from the grid (you must be registered on the grid),
# before to launch the script, do:
# voms-proxy-init --voms vo.padme.org
#
# Run as (put all the runs you want):
# ./CreateListFiles.sh run_0000000_20190301_062658 run_0000000_20190205_111646 run_0000000_20190301_071513 -s
############################


# Color definition
RED='\033[0;31m'
GREEN='\033[0;32m'
NC='\033[0m' # No Color

# default source
indir="root://atlasse.lnf.infn.it:1094//vo.padme.org/"
Source=DEFAULT

opt="0"
for arg in "$@";do
    if [ "$arg" = "-s" ]
    then
	opt="s"

    elif [ "$arg" = "-o" ]
    then
	opt="o"

    elif [ "$arg" = "-a" ]
    then
	opt="a"

    elif [ "$arg" = "LNF" ]
    then
	Source=LNF
	indir="srm://atlasse.lnf.infn.it:8446/srm/managerv2?SFN=/dpm/lnf.infn.it/home/vo.padme.org/"

    elif [ "$arg" = "LNF2" ]
    then
	Source=LNF2
	indir="srm://atlasse.lnf.infn.it:8446/srm/managerv2?SFN=/dpm/lnf.infn.it/home/vo.padme.org_scratch/"

    elif [ "$arg" = "CNAF" ]
    then
	Source=CNAF
	indir="srm://storm-fe-archive.cr.cnaf.infn.it:8444/srm/managerv2?SFN=/padmeTape/"

    elif [ "$arg" = "LOCAL" ]
    then
	Source=LOCAL
	indir="/data05/padme/leonardi/rawdata/"
    fi
done

echo
echo source is set to $Source, namely $indir
if [ "$Source"!="DEFAULT" ] && [ "$Source"!="LOCAL" ]
then
    echo -e "${RED}The produced list will not work with the standard analysis.${NC}\n"
fi


# if not present, create directory cosmics and the sub-directory lists
outpath=$(pwd)/../../cosmics/lists

if [ ! -d "$outpath" ]
then
    echo -e  "\n${GREEN}Creating directory $outpath${NC}\n\n"
    $(mkdir $(pwd)/../../cosmics)
    $(mkdir $(pwd)/../../cosmics/lists)
fi


#if [ "$Source"!="LOCAL" ]
#then
#    voms-proxy-init --voms vo.padme.org
#fi


# loop over input arguments
for RUN in "$@";do

    # Check if it is a run or an input option using regular expression
    regex="^run_[0-9]+_([0-9][0-9][0-9][0-9])[0-9]+_[0-9]+$"
    if [[ $RUN =~ $regex ]]; then
	echo -e "\n========================================="
	year="${BASH_REMATCH[1]}"
	#echo "The run is from year $year"
	
	path="$indir/daq/$year/rawdata/$RUN"
	echo path is $path

        # Run directory not found
	if [ ! 1 ]
	then
	    echo -e  "\n${RED}Directory $indir$RUN does not exist: corresponding file will NOT be created.${NC}\n"
	    
        # Run directory found
	else
	    outlist=$outpath/$RUN.list
	    echo -e "\n${GREEN}Writing in $outlist${NC}\n"
	    
	    if [ -f $outlist ]
	    then
		ans=$opt
		while [ "$ans" != "s" ] && [ "$ans" != "o" ] && [ "$ans" != "a" ]; do
		    echo -e "\n${RED}$outlist already exists."
		    echo -e "\nSkip, Overwrite or Append (WARNING: will NOT check if a file is already present in the list)? [s/o/a]${NC}"
		    read ans
		    echo -e "\n"
		done # end of while over ans
		
		if [ "$ans" = "s" ]
		then
	 	    # Skip this run
		    echo "Skipping this run: old list file will be kept."
		    echo ""
		fi
		
		if [ "$ans" = "o" ]
		then
		    echo -e "Deleting $outlist\n"
		    $(rm $outlist)
                    # Write the output file list
		    echo "Re-writing $outlist"
		    #echo $(ls -d -1 $indir$RUN/*.root >> $outlist)
		    echo $(gfal-ls $path >> $outlist)
		    sed -i 's!^!'"$path"'/!' $outlist
		    #echo $(gfal-ls "$indir/daq/$year/rawdata/$RUN")
		fi
		
		if [ "$ans" = "a" ]
		then
                    # Append in the output file list
		    echo "Appending in $outlist"
		    #echo $(ls -d -1 $indir$RUN/*.root >> $outlist)
		    echo $(gfal-ls "$indir/daq/$year/rawdata/$RUN" >> $outlist)
		    sed -i 's!^!'"$path"'/!' $outlist
		fi
	    

	    else
                # create list for non already existing files
		#echo $(ls -d -1 $indir$RUN/*.root >> $outlist)
		echo $(gfal-ls "$indir/daq/$year/rawdata/$RUN" >> $outlist)
		sed -i 's!^!'"$path"'/!' $outlist

	    fi # end of if on output list file existence
	
	fi # end of if-else on run directory

    #elif [ "$RUN" != "-s" ] && [ "$RUN" != "-o" ] && [ "$RUN" != "-a" ]
    #then
	#echo -e "\n========================================="
	#echo -e  "\n${RED}Directory $indir$RUN does not exist: corresponding file will NOT be created.${NC}\n"
    fi # end of if-elif to skip -s, -o, -a options and non-existing path

done # end of for over RUN

echo ""