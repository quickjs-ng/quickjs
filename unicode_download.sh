#!/bin/sh
set -e

files="CaseFolding.txt DerivedNormalizationProps.txt PropList.txt \
SpecialCasing.txt CompositionExclusions.txt ScriptExtensions.txt \
UnicodeData.txt DerivedCoreProperties.txt NormalizationTest.txt Scripts.txt \
PropertyValueAliases.txt"

mkdir -p unicode

for f in $files; do
    wget "https://www.unicode.org/Public/15.0.0/ucd/${f}" -O unicode/$f
done
    
wget "https://www.unicode.org/Public/15.0.0/ucd/emoji/emoji-data.txt" -O unicode/emoji-data.txt
wget "https://www.unicode.org/Public/emoji/15.0/emoji-sequences.txt" -O unicode/emoji-sequences.txt
wget "https://www.unicode.org/Public/emoji/15.0/emoji-zwj-sequences.txt" -O unicode/emoji-zwj-sequences.txt
