#!/usr/bin/perl

print "Content-Type: text/html;\r\n";
print "\r\n";

$buildlevel=do"./buildlevel";

print <<'---'=~s/__buildlevel__/$buildlevel/gr;
<!DOCTYPE html>
<html>

<head>
<meta http-equiv="CONTENT-TYPE" content="text/html; charset=UTF-8">
<title>Nibbles Commenter for Anarchy Golf</title>
</head>

<body>
<h1>Nibbles Commenter for Anarchy Golf</h1>
<p>
Nibbles Commenter will help you to decipher <a href="http://golfscript.com/nibbles/" target="_blank">Nibbles</a> codes posted on <a href="http://golf.shinh.org/" target="_blank">Anarchy Golf</a>.
</p>
<p>
Beta version. (Commenter version 0.1.3.__buildlevel__)
</p>
<!--
<p>
Known bugs:
</p>
<ul>
<li>(2022-03-04) I decided not to fix the arg usedness issue (the difference of behavior between Nibbles and Commenter), first because the behavior of Nibbles &lt;= 0.24 is too hard to imitate, and second because Darren has already fixed the Nibbles behavior in the codebase. Now that Commenter has no known major bugs and no pending to-do, it is in Beta. Congratulations!</li>
<li>(2022-02-17) Determination of usedness of arguments is still incorrect in version 0.0.7.*. I can't fix it because I don't get the rule, especially why <br><code>/ , 2 - $ - ct</code><br> says <code>$</code> is still unused (and thus prioritized). ???</li>
</ul>
-->
<p>
Recent updates:
</p>
<ul>
<li>(2024-11-30)(0.1.3.183) Fixed tuple coerce2 bugs</li>
<li>(2024-04-19)(0.1.2.181) Fixed misinterpretation of <code>~</code> (auto/tuple/option) in some cases</li>
<li>(2024-04-03)(0.1.1.165) Fixed that the resulting type of <code>+ 'a' ~</code> was int</li>
<li>(2024-02-02)(0.1.0.163) Changed the description of <code>?,</code> from "if nonnull" to "if/else (lazy list)" on accordance with <a href="http://golfscript.com/nibbles/quickref.html">Nibbles Quick Ref</a></li>
<li>(2024-01-22)(0.1.0.162) Fixed that <code>?</code> ... <code>~</code> (index by) was not handled.<!-- Now <code>?</code> ... <code>~ ~ ~</code> is recognized as: index by not tuple --></li>
<li>(2024-01-09)(0.1.0.160) Fixed the return type of binary list ops (<code>`&amp;</code>, <code>`|</code>, <code>`^</code>, <code>`-</code>). <!-- It is now coerce2(type of 1st list, type of 2nd list) regardless whether the uniq option and/or the optional mapping function exists. Hope that is correct, though I have not fully understood the rule. --></li>
<li>(2022-06-26)(0.1.0.157) Catching up with Nibbles version 1.00</li>
<!--
<li>(2022-06-26)(0.1.0.157) Catching up with Nibbles version 1.00:
<ol>
<li>New nibble sequence for <code>``p</code> (permutations) and <code>`&lt;</code> (take also drop)</li>
</ol>
</li>
<li>(2022-03-05)(0.1.0.120) Catching up with Nibbles version 0.25:
<ol>
<li>Swapping <code>;@</code> and <code>;;@</code> (allLines and sndLine) at the initial state</li>
<li>Adding <code>``@</code> (to bits) op</li>
<li>Reverting to the string encoding rule used in versions &lt;= 0.23</li>
</ol>
</li>
<li>(2022-03-04) Providing raw data in JSON</li>
<li>(2022-02-28) Compact mode (just for veterans)</li>
<li>(2022-02-25)(0.0.7.102) Fixed <code>`%</code> (split list) taking 2nd argument of any type including <code>~</code> (auto) in versions &lt;0.24. This is only allowd in versions &gt= 0.24.</li>
<li>(2022-02-25)(0.0.7.99) Catching up with Nibbles version 0.24: Changing string literal format.</li>
<li>(2022-02-24)(0.0.7.91) Catching up with Nibbles version 0.24:
<ol>
<li>Removal of <code>foldr</code>/<code>scanl</code> list tuple initial value specifier <code>~</code>.</li>
<li>Changing return type of <code>error</code>.</li>
</ol>
Please tell me if you find any other changes that Commenter should support.
</li>
<li>(2022-02-23)(0.0.7.57) Better conformance to Nibbles official conventions for naming <code>`;</code> (recursion) arguments, althoug the names-style .nbb cannot be recompiled since you can't call a fn by name [(2022-02-24) That seems fixed in Nibbles 0.24]. To recompile, you can use the DeBruijn style instead [(2022-02-24) Or use Nibbles 0.24].</li>
<li>(2022-02-17)(0.0.7.5) Fixed let arg scope bug.</li>
-->
</ul>
<p>
Please report any bugs you find so I'll fix them.
</p>
---

{
    local $h;
    local $/=undef;
    open($h, 'curl -s http://golf.shinh.org/l.rb?nbb|');
    $_ = <$h>;
}

while( m{<h2><a href="/p.rb\?[^"]*">[^<>]*</a> \(post mortem\)</h2><table border="1">.*?</table>}g ) {
    $e = $&;
    $e =~ s{ \(post mortem\)}{};
    $e =~ s{<a href="bas.html">Statistics</a>}{Statistics};
    $e =~ s{/p.rb}{http://golf.shinh.org$&};
    $e =~ s/reveal\.rb/commenter.pl/g;
    print $e,"\n";
}
m{<p>Last update: .*?</p>};
print $&,"\n";

print <<'---';
<p><a href="http://golf.shinh.org/">return to the top page of Anarchy Golf</a></p>
<hr>
<p>
Nibbles Commenter for Anarchy Golf by <a href="https://twitter.com/saito_ta" target="_blank">tails</a>.
</p>
<p>
<a href="https://github.com/saito-ta/nbbdag">Source code.</a>
</p>
<p>
Acknowledgement:<br>
shinh : <a href="http://golf.shinh.org/" target="_blank">Anarchy Golf</a><br>
Darren Smith <!-- ("flagitious"? not sure.) -->: <a href="http://golfscript.com/nibbles/" target="_blank">Nibbles</a><br>
</p>
<!--
<hr>
<p>
(empty paragraph.)
</p>
-->
</body></html>
---
