if [[ -e ".local" ]] ; then
	echo $[1+`<buildlevel-local`] >buildlevel-local
fi
cp buildlevel-local buildlevel
