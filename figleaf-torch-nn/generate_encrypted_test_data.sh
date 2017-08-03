#!/usr/bin/zsh
#--------------------------------------------------------------------
# This is a simple script to generate encrypted figleaf test data. It is not
# really specific to the neural network tool.
#
# This script requires editing hardcoded paths, it does not take them as
# arguments
#
# This script written by Byron Marohn for a 2017 independent study with Dr.
# Charles Wright at Portland State University.
#
#--------------------------------------------------------------------

### Convert input images to JPEG
rm -f tmp.parallel
find ./ATT -name '*.pgm' | while read file; do
	filenum="${file:t:r}"
	filedir="${file:h}"
	#echo "$filenum"
	#echo "$filedir"
	for quality in 50 70 95; do
		echo "convert '$file' -quality '$quality' '$filedir/${filenum}_quality_${quality}.jpg'" >> tmp.parallel
	done
done
parallel -j 16 < tmp.parallel
rm -f tmp.parallel

### Encrypt the resulting images
rm -f tmp.parallel
for quality in 50 70 95; do
	for bits in 0 1 3 6; do
		for block in 8 16 24 32; do
			outputdir="ATT_encrypted/quality_${quality}_bits_${bits}_block_${block}"
			mkdir -p "$outputdir"
			find ./ATT -name "*quality_${quality}.jpg" | while read file; do
				filename="${file:t}"
				persondir="${file:h:t}"
				mkdir -p "$outputdir/$persondir"
				echo "../figleaf/figleaf -e -i '$(pwd)/$file' -o '$(pwd)/$outputdir/$persondir/$filename' -p 'figleaf' -b '$block' -s -a '$bits'" >> tmp.parallel
			done
		done
	done
done
parallel -j 16 < tmp.parallel
rm -f tmp.parallel

