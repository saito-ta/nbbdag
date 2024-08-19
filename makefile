nbb-commenter : nbb-commenter.cpp
	bash update-buildlevel.sh
	g++ -O2 $< -o $@
	rm -f commented-json/*
