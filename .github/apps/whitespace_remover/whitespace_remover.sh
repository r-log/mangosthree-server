find -name '*.cpp' -print0 | xargs -r0 sed -e 's/[[:blank:]]\+$//' -i
find -name '*.h' -print0 | xargs -r0 sed -e 's/[[:blank:]]\+$//' -i

