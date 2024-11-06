#!/usr/bin/perl

use JSON;

sub escape {
	local($_)=@_;
	s/&/&amp;/g;
	s/</&lt;/g;
	s/>/&gt;/g;
	s/"/&quot;/g;
	$_;
}

sub slurp {
    local $/;
    my $h;
    open($h,$_[0]) or die "slurp: <$_[0]>";
    my $r = readline($h);
    close($h);
    $r;
}

$subid = $ENV{QUERY_STRING};
$format = $subid =~ s/&format=((\w|-)*)// && $1;
if($format ne "compact" && $format ne "simple" && $format ne "type" && $format ne "table" && $format ne "json" && $format ne "nbl" && $format ne "nbl-names"){
    $format = "simple";
}
    
$codever= $subid =~ s/&codever=(\w*)// ? $1 : "";
$subid =~ s/&.*//;
$subid =~ /(.*)\//;
if( not $codever eq "020" || $codever eq "021" || $codever eq "022" || $codever eq "023" || $codever eq "024" || $codever eq "025" || $codever eq "100" ){
	$codever = "100";
	if($subid=~/_(\d{10})$/){
		my $t=$1;
		if($t lt "1640321052"){
			# http://www.tailsteam.com/cgi-bin/nbbdag/commenter.pl?Permutations/Dingledooper_1640321051&nbb
			$codever="020";
		}
		elsif($t lt "1642477454"){
			# http://www.tailsteam.com/cgi-bin/nbbdag/commenter.pl?Twelve/tails_1642477453&nbb
			# http://www.tailsteam.com/cgi-bin/nbbdag/commenter.pl?Truth+Machine/darrenks_1641348003&nbb
			$codever="021";
		}
		elsif($t lt "1645704228"){
			$codever="023";
		}
		elsif($t lt "1646439805"){
			$codever="024";
		}
		elsif($t lt "1656496700"){
			$codever="025";
		}
	}
}
$probid = $1;
die "illegal query: probid=<$probid>" if $probid !~ /^(%[0-9A-F]{2}|[-+_0-9A-Za-z])+$/;
die "illegal query: subid=<$subid>" if $subid !~ /\/(%[0-9A-F]{2}|[-+_0-9A-Za-z.])+$/;

$subfn = "commented-static/" . $subid =~ s'/'='gr . "=$codever";
if( ! -f $subfn ) {
    $subfn = "commented-json/" . $subid =~ s'/'='gr . "=$codever";
    if(!-f $subfn ) {
		$nbbfn="nbb/".$subid=~ s'/'='gr;
		if(!-f $nbbfn ){
	        system("curl -s \"http://golf.shinh.org/reveal.rb?$subid/plain\" >$nbbfn");
		}
        system("timeout -s 9 60 ./nbb-commenter JSON $codever <$nbbfn >\"$subfn\"");
    }
}
$_= slurp($subfn);

if($format eq "json"){
	print "Content-Type: text/plain;\r\n";
	print "\r\n";
	print;
	exit;
}

#$_=eval; # pson
#$pson_error=$@;
eval{$json=decode_json($_);};
$_=$json;

sub show_nibbles_version{
	my($v)=@_;
	$v eq "0.20" ? "0.2" :
	$v; # "unknown";
}

sub print_nbl_subtree{
	my($n,$level)=@_;
	print "    "x$level;
	if(exists $n->{lit}){
		print $n->{lit}," ";
	}
	print "#";
	if(exists $n->{desc}){
		print "(",$n->{desc},"):",$n->{type}," ";
	}
	if(exists $n->{args}){
		print "<--arg( ";
		for(@{$n->{args}}){
			print $_->{desc},":",$_->{type}," ";
		}
		print ") ";
	}
	if(exists $n->{lets}){
		print "-->let( ";
		for(@{$n->{lets}}){
			print $_->{desc},":",$_->{type}," ";
		}
		print ") ";
	}
	print "\n";
	for(@{$n->{childs}}){
		print_nbl_subtree($_,$level+1);
	}
}

sub print_nbl_names_subtree{
	my($n,$level)=@_;
	if(exists $n->{args}){
		print "    "x$level;
		if($n->{desc}=~/^implicit arg /){
			print "#";
		}
		print "\\";
		for(@{$n->{args}}){
			print $_->{desc}," ";
		}
		print "\n";
	}
	print "    "x$level;
	if($n->{desc}=~/^= (\w+)$/){
		print $1," #(",$n->{lit},")";
	}
	else{
		if(exists $n->{lit}){
			print $n->{lit}," ";
		}
		print "#";
		if(exists $n->{desc}){
			print "(",$n->{desc},")";
		}
	}
	print ":",$n->{type}," ";
	print "\n";
	for(@{$n->{childs}}){
		print_nbl_names_subtree($_,$level+1);
	}
	if(exists $n->{lets}){
		print "    "x$level;
		if($n->{passed}){
			print "#";
		}
		print "sets ";
		for(@{$n->{lets}}){
			print $_->{desc}," ";
		}
		print "\n";
	}
}

if($format eq "nbl" || $format eq "nbl-names"){
	print "Content-Type: text/plain;\r\n";
	print "\r\n";
	if($pson_error){
		print "### internal error: ",$pson_error,"\n";
	}
	print "# code: http://golf.shinh.org/reveal.rb?$subid&nbb\n";
	print "# target Nibbles version: ",show_nibbles_version($_->{nibbles_version}),"\n";
	print "# commenter version: ",$_->{commenter_version},"\n";
	print "\n";
	if($format eq "nbl-names"){
		print_nbl_names_subtree($_->{code},0);
	}
	else{
		print_nbl_subtree($_->{code},0);
	}
	if($_->{postdata}){
		if(exists $_->{postdata}{lit}){
			print $_->{postdata}{lit}," ";
		}
		print "#(postdata (",$_->{postdata}{format},"): ",$_->{postdata}{desc},")\n";
		print $_->{postdata}{data},"\n";
		if(exists $_->{postdata}{str}){
			print "# ",$_->{postdata}{str},"\n";
		}
	}
	exit;
}

print "Content-Type: text/html;\r\n";
print "\r\n";

{
	print "<p>Problem: <a href=\"http://golf.shinh.org/p.rb?$probid\">",$probid=~y/+/ /r,"</a></p>";
	print "<p>Code: <a href=\"http://golf.shinh.org/reveal.rb?$subid&nbb\">$subid</a></p>";
	print
		"<p>Nibbles version: ",
		$codever eq "020" ? "<b>0.2</b>" : "<a href=\"commenter.pl?$subid&format=$format&codever=020\">0.2</a>",
		" | ",
		$codever eq "021" ? "<b>0.21</b>" : "<a href=\"commenter.pl?$subid&format=$format&codever=021\">0.21</a>",
		" | ",
		$codever eq "022" ? "<b>0.22</b>" : "<a href=\"commenter.pl?$subid&format=$format&codever=022\">0.22</a>",
		" | ",
		$codever eq "023" ? "<b>0.23</b>" : "<a href=\"commenter.pl?$subid&format=$format&codever=023\">0.23</a>",
		" | ",
		$codever eq "024" ? "<b>0.24</b>" : "<a href=\"commenter.pl?$subid&format=$format&codever=024\">0.24</a>",
		" | ",
		$codever eq "025" ? "<b>0.25</b>" : "<a href=\"commenter.pl?$subid&format=$format&codever=025\">0.25</a>",
		" | ",
		$codever eq "100" ? "<b>1.00</b>" : "<a href=\"commenter.pl?$subid&format=$format&codever=100\">1.00</a>",
		"</p>";
	print
		"<p>Format: ",
		$format eq "compact" ? "<b>Compact</b>" : "<a href=\"commenter.pl?$subid&format=compact&codever=$codever\">Compact</a>",
		" | ",
		$format eq "simple" ? "<b>Simple</b>" : "<a href=\"commenter.pl?$subid&format=simple&codever=$codever\">Simple</a>",
		" | ",
		$format eq "type" ? "<b>+Type</b>" : "<a href=\"commenter.pl?$subid&format=type&codever=$codever\">+Type</a>",
		" | ",
		$format eq "table" ? "<b>Table</b>" : "<a href=\"commenter.pl?$subid&format=table&codever=$codever\">Table</a>",
		" | ",
		$format eq "json" ? "<b>JSON</b>" : "<a href=\"commenter.pl?$subid&format=json&codever=$codever\">JSON</a>",
		" | ",
		$format eq "nbl" ? "<b>.nbl (DeBruijn)</b>" : "<a href=\"commenter.pl?$subid&format=nbl&codever=$codever\">.nbl (DeBruijn)</a>",
		" | ",
		$format eq "nbl-names" ? "<b>.nbl (names)</b>" : "<a href=\"commenter.pl?$subid&format=nbl-names&codever=$codever\">.nbl (names)</a>",
		"</p>";
	print "<hr>";
}

if($pson_error){
	print "<font color=\"red\">",escape($pson_error),"</font>";
}


sub print_compact_subtree{
	my($n,$level)=@_;
	my $lit='';
	if(exists $n->{lit}){
		$lit=$n->{lit}." ";
	}
	print escape($lit);
	$level+=length($lit);
	my $first=1;
	for(@{$n->{childs}}){
		if($first){
			$first=0;
		}
		else{
			print "\n"," "x$level;
		}
		print_compact_subtree($_,$level);
	}
}

sub print_plain_subtree{
	my($n,$level)=@_;
	print "    "x$level;
	if(exists $n->{lit}){
		print escape($n->{lit})," ";
	}
	print "<font color=\"green\">";
	if(exists $n->{desc}){
		if($n->{desc}=~/^(implicit arg |)= (\w+)$/){
			print "($1= <font color=\"red\">$2</font>)";
		}
		else{
			print "(",escape($n->{desc}),")";
		}
	}
	if($format eq "type"){
		print ":<font color=\"blue\">",$n->{type},"</font>";
	}
	print " ";
	if(exists $n->{args}){
		print "&lt;--arg( ";
		for(@{$n->{args}}){
			print "<font color=\"red\">",escape($_->{desc}),"</font>";
			if($format eq "type"){
				print ":<font color=\"blue\">",$_->{type},"</font>";
			}
			print " ";
		}
		print ") ";
	}
	if(exists $n->{lets}){
		print "--&gt;let( ";
		for(@{$n->{lets}}){
			print "<font color=\"red\">",escape($_->{desc}),"</font>";
			if($format eq "type"){
				print ":<font color=\"blue\">",$_->{type},"</font>";
			}
			print " ";
		}
		print ") ";
	}
	print "</font>\n";
	for(@{$n->{childs}}){
		print_plain_subtree($_,$level+1);
	}
}

if($format eq "compact" || $format eq "simple" || $format eq "type"){
	print "<html>";
	if(defined $_){
		if(exists $_->{note}){
			print "<p><font color=\"red\">Note: ",escape($_->{note}),"</font></p>";
		}
		print "<pre>";
		if($format eq "compact"){
			print_compact_subtree($_->{code});
			print "\n";
			if($_->{postdata}){
				print $_->{postdata}{data},"\n";
			}
		}
		else{
			print_plain_subtree($_->{code});
			if($_->{postdata}){
				if(exists $_->{postdata}{lit}){
					print $_->{postdata}{lit}," ";
				}
				print "<font color=\"green\">(postdata (",$_->{postdata}{format},"): ",escape($_->{postdata}{desc}),")</font>\n";
				print $_->{postdata}{data},"\n";
				if(exists $_->{postdata}{str}){
					print "<font color=\"green\">(",escape($_->{postdata}{str}),")</font>\n";
				}
			}
		}
		print "</pre>";
	}
	else{
		print "<p>Cannot parse the code in the specified version.</p>"
	}
}

sub print_table_subtree{
	my($n,$level)=@_;
	print "<tr>";

	# Depth
	print "<td>";
	print "*"x$level," ",$level;
	print "</td>";

	# Lit
	print "<td><code>";
	if(exists $n->{lit}){
		print escape($n->{lit})," ";
	}
	print "</code></td>";

	# Desc
	print "<td>";
	print "<font color=\"green\">";
	if(exists $n->{desc}){
		if($n->{desc}=~/^(implicit arg |)= (\w+)$/){
			print "$1= <font color=\"red\">$2</font>";
		}
		else{
			print escape($n->{desc});
		}
	}
	print "</font></td>";

	# Type
	print "<td>";
	print "<font color=\"blue\">",$n->{type},"</font>";
	print "</td>";

	# Arg
	print "<td>";
	if(exists $n->{args}){
		for(@{$n->{args}}){
			print "<font color=\"red\">",escape($_->{desc}),"</font>";
			print ":<font color=\"blue\">",$_->{type},"</font>";
			print " ";
		}
	}
	print "</td>";

	# Let
	print "<td>";
	if(exists $n->{lets}){
		for(@{$n->{lets}}){
			print "<font color=\"red\">",escape($_->{desc}),"</font>";
			print ":<font color=\"blue\">",$_->{type},"</font>";
			print " ";
		}
	}
	print "</td>";

	print "</tr>";
	for(@{$n->{childs}}){
		print_table_subtree($_,$level+1);
	}
}

if($format eq "table"){
	print "<table border=\"1\" cellspacing=\"0\">";
	print "<tr><th>Depth</th><th>Lit</th><th>Desc</th><th>Type</th><th>Arg</th><th>Let</th></tr>";
	print_table_subtree($_->{code},0);
	print "</table>";
}

print "<hr><a href=\"index.pl\">Back to the index</a></html>";
