#!/bin/bash

# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

CONTENTS=$(egrep '^#.*$' < $1 \
           | sed -n -e '1,/^### Begin of instructions/d' \
                    -e '1,/^### End of instructions/p')

SUBSECTIONS=$(echo "$CONTENTS" | egrep '^## .*' \
              | sed -e 's/## \(.*\)/\1/')

# Menu of subsections.

echo "@menu"
while read -r sub
do
    echo "* $sub::"
done <<< "$SUBSECTIONS"
echo "@end menu"

# Process each subsection.

while read -r sub
do
    sed_cmd='/^## '$sub'/,/^## /p'
    SUBCONTENTS=$(echo "$CONTENTS" \
                  | sed -n -e "$sed_cmd" \
                  | head --line=-1)

    echo ""
    echo "@node $sub"
    echo "@subsection $sub"
    echo ""

    # Instructions menu
    echo "@menu"
    echo "$SUBCONTENTS" \
        | egrep '^# Instruction: ' \
        | sed -e 's/# Instruction: \([^ ][^ ]*\)\(.*\)/* Instruction \1::/g'
    echo "@end menu"

    # Instruction subsections
    echo "$SUBCONTENTS" \
        | sed -e 's/## .*//' \
              -e 's/# Stack: \(.*\)/\nStack: @code{\1}/' \
              -e 's/# Exceptions Stack: \(.*\)/\nException Stack: @code{\1}/' \
              -e 's/# Exceptions: \(.*\)/\nExceptions: @code{\1}/' \
              -e 's/# Instruction: \([^ ][^ ]*\)\(.*\)/\n@node Instruction \1\n@subsubsection Instruction \1\n\nSynopsys:\n\n@example\n\1\2\n@end example\n\n/' \
              -e 's/^# //' -e 's/^#//' \
        | uniq
done <<< "$SUBSECTIONS"
