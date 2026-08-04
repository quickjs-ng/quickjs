#!/bin/sh
set -e

baseurl="https://www.unicode.org"
license_url="${baseurl}/license.txt"
version="17.0.0"
url="${baseurl}/Public/${version}/ucd"
emoji_url="${url}/emoji/emoji-data.txt"
emoji_seq_url="${baseurl}/Public/${version}/emoji"

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
wget "${emoji_seq_url}/emoji-sequences.txt" -O unicode/emoji-sequences.txt
wget "${emoji_seq_url}/emoji-zwj-sequences.txt" -O unicode/emoji-zwj-sequences.txt
wget $license_url -O unicode/license.txt
