CS 61 Problem Set 5
===================

**Fill out both this file and `AUTHORS.md` before submitting.** We grade
anonymously, so put all personally identifying information, including
collaborators, in `AUTHORS.md`.

Grading notes (if any)
----------------------
I prefer that you grade my previous commit for normal grading and then this commit for extra credit, but it honestly doesn't matter too much as I still pass all tests.
I try to mark everything by phase (even extra credit), so if you get lost you can always control f.
Thank you so much!


Extra credit attempted (if any)
-------------------------------

I implemented variable substitution. I read this wikibooks page as linked in the pset to see what to do.
https://en.wikibooks.org/wiki/Bourne_Shell_Scripting/Variable_Expansion 

I didn't implement everything, but I implemented:

$NAME where NAME matches [A-Za-z_][A-Za-z0-9_]*
${NAME} with braces for disambiguation

I wrote some checks for myself to check my implementation. You can control F extra credit in check.pl or look at the bottom of the checks.
