#!/usr/bin/zsh
#--------------------------------------------------------------------
# This is a simple script which generates a second script which is run to
# perform multiple experiments using the figleaf neural network classifier.
# This output script, alltasks.sh, will contain two lines per experiment - one
# to run the experiment, and a second to log completion of that command into
# done_steps.txt. This isn't particularly pretty, but it allows you to run
# alltasks.sh, then later control+c it to kill the experiment, then look at the
# log and see how far it got and resume the experiment where it was left off by
# editing alltasks.sh appropriately (to delete lines already executed).
#
# This script written by Byron Marohn for a 2017 independent study with Dr.
# Charles Wright at Portland State University.
#
#--------------------------------------------------------------------

INDIR="./ATT_encrypted"
OUTDIR="./out"
mkdir -p "$OUTDIR"
rm -f alltasks.sh
rm -f done_steps.txt
for file in "$INDIR/"*; do
   for x in 1 2 3 4 5 6 7 8 9 10; do
      mkdir -p "$OUTDIR/$file/$x"
      #command="qlua train-on-mnist.lua  --load $file --save "$OUTDIR/$file/$x" --maxEpoch 150 -x $x"
      command="qlua figleaf-nn-train.lua  --load $file --save "$OUTDIR/$file/$x" --maxEpoch 150 -x $x"
      echo "$command" >> alltasks.sh
      echo "echo $command >> done_steps.txt" >> alltasks.sh
   done
done



