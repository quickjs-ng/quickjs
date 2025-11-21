#!/bin/sh
set -e

url="https://www.unicode.org/Public/17.0.0/ucd"
emoji_url="${url}/emoji/emoji-data.txt"

files="CaseFolding.txt DerivedNormalizationProps.txt PropList.txt \
SpecialCasing.txt CompositionExclusions.txt ScriptExtensions.txt \
UnicodeData.txt DerivedCoreProperties.txt NormalizationTest.txt Scripts.txt \
PropertyValueAliases.txt"

mkdir -p unicode

for f in $files; do
    g="${url}/${f}"
    wget $g -O unicode/$f
done
    
wget $emoji_url -O unicode/emoji-data.txt
