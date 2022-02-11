TIMEFORMAT="%E" # the script breaks if the program ever takes longer than 1 or 2 sec to run, because output style of time will change and the awk script won't accept it

function validateTestResult {
	if [[ -z $(git diff --no-index $1 $2) ]]
	then
		echo "test passed"; return
	fi
	echo "TEST FAILED"
	# do some nfilediffing here as soon as you get a linux version of nfilediff up and running.
	exit
}

index=0
longestDurationIndex=-1
longestDuration=-1
function test {
	duration=$({ time cat inputs/source.cpp | ../a.out $@ > output.txt; } 2>&1)

	echo -n "$index  $duration  "

	if (( $(echo "$duration $longestDuration" | awk '{print ($1 > $2)}') ))
	then
		longestDuration=$duration
		longestDurationIndex=$index
	fi

	validateTestResult "templates/$index.txt" "output.txt"

	(( index=$index+1 ))
}

test a
test -a a
test -c a
test -ca a
test -l a
test -al a
test -cal a
test -cl a

test -v a
test -v -a a
test -v -c a
test -v -ca a
test -v -l a
test -v -al a
test -v -cal a
test -v -cl a

test --context 5 a
test --context 5 -a a
test --context 5 -c a
test --context 5 -ca a
test --context 1 -l a
test --context 1 -al a
test --context 1 -cal a
test --context 1 -cl A

test --context 5 -v a
test --context 5 -v -a a
test --context 1 -v -c a
test --context 1 -v -ca a
test --context 1 -v -l a
test --context 1 -v -al a
test --context 1 -v -cal a
test --context 1 -v -cl a

test --only-line-nums a
test --only-line-nums -a a
test --only-line-nums -c a
test --only-line-nums -ca a
test --only-line-nums -l a
test --only-line-nums -al a
test --only-line-nums -cal a
test --only-line-nums -cl a

test --only-line-nums -v a
test --only-line-nums -v -a a
test --only-line-nums -v -c a
test --only-line-nums -v -ca a
test --only-line-nums -v -l a
test --only-line-nums -v -al a
test --only-line-nums -v -cal a
test --only-line-nums -v -cl a

test --only-line-nums --context 5 a
test --only-line-nums --context 5 -a a
test --only-line-nums --context 5 -c a
test --only-line-nums --context 5 -ca a
test --only-line-nums --context 1 -l a
test --only-line-nums --context 1 -al a
test --only-line-nums --context 1 -cal a
test --only-line-nums --context 1 -cl A

test --only-line-nums --context 5 -v a
test --only-line-nums --context 5 -v -a a
test --only-line-nums --context 1 -v -c a
test --only-line-nums --context 1 -v -ca a
test --only-line-nums --context 1 -v -l a
test --only-line-nums --context 1 -v -al a
test --only-line-nums --context 1 -v -cal a
test --only-line-nums --context 1 -v -cl a

test --color on a
test --color on -a a
test --color on -c a
test --color on -ca a
test --color on -l a
test --color on -al a
test --color on -cal a
test --color on -cl a

test --color on -v a
test --color on -v -a a
test --color on -v -c a
test --color on -v -ca a
test --color on -v -l a
test --color on -v -al a
test --color on -v -cal a
test --color on -v -cl a

test --color on --context 5 a
test --color on --context 5 -a a
test --color on --context 5 -c a
test --color on --context 5 -ca a
test --color on --context 1 -l a
test --color on --context 1 -al a
test --color on --context 1 -cal a
test --color on --context 1 -cl A

test --color on --context 5 -v a
test --color on --context 5 -v -a a
test --color on --context 1 -v -c a
test --color on --context 1 -v -ca a
test --color on --context 1 -v -l a
test --color on --context 1 -v -al a
test --color on --context 1 -v -cal a
test --color on --context 1 -v -cl a

test --color on --only-line-nums a
test --color on --only-line-nums -a a
test --color on --only-line-nums -c a
test --color on --only-line-nums -ca a
test --color on --only-line-nums -l a
test --color on --only-line-nums -al a
test --color on --only-line-nums -cal a
test --color on --only-line-nums -cl a

test --color on --only-line-nums -v a
test --color on --only-line-nums -v -a a
test --color on --only-line-nums -v -c a
test --color on --only-line-nums -v -ca a
test --color on --only-line-nums -v -l a
test --color on --only-line-nums -v -al a
test --color on --only-line-nums -v -cal a
test --color on --only-line-nums -v -cl a

test --color on --only-line-nums --context 5 a
test --color on --only-line-nums --context 5 -a a
test --color on --only-line-nums --context 5 -c a
test --color on --only-line-nums --context 5 -ca a
test --color on --only-line-nums --context 1 -l a
test --color on --only-line-nums --context 1 -al a
test --color on --only-line-nums --context 1 -cal a
test --color on --only-line-nums --context 1 -cl A

test --color on --only-line-nums --context 5 -v a
test --color on --only-line-nums --context 5 -v -a a
test --color on --only-line-nums --context 1 -v -c a
test --color on --only-line-nums --context 1 -v -ca a
test --color on --only-line-nums --context 1 -v -l a
test --color on --only-line-nums --context 1 -v -al a
test --color on --only-line-nums --context 1 -v -cal a
test --color on --only-line-nums --context 1 -v -cl a

test --color off a
test --color off -a a
test --color off -c a
test --color off -ca a
test --color off -l a
test --color off -al a
test --color off -cal a
test --color off -cl a

test --color off -v a
test --color off -v -a a
test --color off -v -c a
test --color off -v -ca a
test --color off -v -l a
test --color off -v -al a
test --color off -v -cal a
test --color off -v -cl a

test --color off --context 5 a
test --color off --context 5 -a a
test --color off --context 5 -c a
test --color off --context 5 -ca a
test --color off --context 1 -l a
test --color off --context 1 -al a
test --color off --context 1 -cal a
test --color off --context 1 -cl A

test --color off --context 5 -v a
test --color off --context 5 -v -a a
test --color off --context 1 -v -c a
test --color off --context 1 -v -ca a
test --color off --context 1 -v -l a
test --color off --context 1 -v -al a
test --color off --context 1 -v -cal a
test --color off --context 1 -v -cl a

test --color off --only-line-nums a
test --color off --only-line-nums -a a
test --color off --only-line-nums -c a
test --color off --only-line-nums -ca a
test --color off --only-line-nums -l a
test --color off --only-line-nums -al a
test --color off --only-line-nums -cal a
test --color off --only-line-nums -cl a

test --color off --only-line-nums -v a
test --color off --only-line-nums -v -a a
test --color off --only-line-nums -v -c a
test --color off --only-line-nums -v -ca a
test --color off --only-line-nums -v -l a
test --color off --only-line-nums -v -al a
test --color off --only-line-nums -v -cal a
test --color off --only-line-nums -v -cl a

test --color off --only-line-nums --context 5 a
test --color off --only-line-nums --context 5 -a a
test --color off --only-line-nums --context 5 -c a
test --color off --only-line-nums --context 5 -ca a
test --color off --only-line-nums --context 1 -l a
test --color off --only-line-nums --context 1 -al a
test --color off --only-line-nums --context 1 -cal a
test --color off --only-line-nums --context 1 -cl A

test --color off --only-line-nums --context 5 -v a
test --color off --only-line-nums --context 5 -v -a a
test --color off --only-line-nums --context 1 -v -c a
test --color off --only-line-nums --context 1 -v -ca a
test --color off --only-line-nums --context 1 -v -l a
test --color off --only-line-nums --context 1 -v -al a
test --color off --only-line-nums --context 1 -v -cal a
test --color off --only-line-nums --context 1 -v -cl a

test --color auto a
test --color auto -a a
test --color auto -c a
test --color auto -ca a
test --color auto -l a
test --color auto -al a
test --color auto -cal a
test --color auto -cl a

test --color auto -v a
test --color auto -v -a a
test --color auto -v -c a
test --color auto -v -ca a
test --color auto -v -l a
test --color auto -v -al a
test --color auto -v -cal a
test --color auto -v -cl a

test --color auto --context 5 a
test --color auto --context 5 -a a
test --color auto --context 5 -c a
test --color auto --context 5 -ca a
test --color auto --context 1 -l a
test --color auto --context 1 -al a
test --color auto --context 1 -cal a
test --color auto --context 1 -cl A

test --color auto --context 5 -v a
test --color auto --context 5 -v -a a
test --color auto --context 1 -v -c a
test --color auto --context 1 -v -ca a
test --color auto --context 1 -v -l a
test --color auto --context 1 -v -al a
test --color auto --context 1 -v -cal a
test --color auto --context 1 -v -cl a

test --color auto --only-line-nums a
test --color auto --only-line-nums -a a
test --color auto --only-line-nums -c a
test --color auto --only-line-nums -ca a
test --color auto --only-line-nums -l a
test --color auto --only-line-nums -al a
test --color auto --only-line-nums -cal a
test --color auto --only-line-nums -cl a

test --color auto --only-line-nums -v a
test --color auto --only-line-nums -v -a a
test --color auto --only-line-nums -v -c a
test --color auto --only-line-nums -v -ca a
test --color auto --only-line-nums -v -l a
test --color auto --only-line-nums -v -al a
test --color auto --only-line-nums -v -cal a
test --color auto --only-line-nums -v -cl a

test --color auto --only-line-nums --context 5 a
test --color auto --only-line-nums --context 5 -a a
test --color auto --only-line-nums --context 5 -c a
test --color auto --only-line-nums --context 5 -ca a
test --color auto --only-line-nums --context 1 -l a
test --color auto --only-line-nums --context 1 -al a
test --color auto --only-line-nums --context 1 -cal a
test --color auto --only-line-nums --context 1 -cl A

test --color auto --only-line-nums --context 5 -v a
test --color auto --only-line-nums --context 5 -v -a a
test --color auto --only-line-nums --context 1 -v -c a
test --color auto --only-line-nums --context 1 -v -ca a
test --color auto --only-line-nums --context 1 -v -l a
test --color auto --only-line-nums --context 1 -v -al a
test --color auto --only-line-nums --context 1 -v -cal a
test --color auto --only-line-nums --context 1 -v -cl a

# additional testing of a couple error messages
test
# we're not going to test the help text because that would mean regenerating templates everytime I change the help text, which would be annoying
#test --help
#test --h
test a a
test a --context 5
test --context 1000000 a
test --color invalidvalue a

if [[ $longestDuration != "-1" ]]
then
	echo "test index with the longest duration: $longestDurationIndex,     had this duration: $longestDuration"
fi
