**Initial Requirement**

```
Hi

John mentioned that in stand-up that he has had a conversation with an external (?) party along the lines that it would be nice to have a high-level script language.

One of the requirements for this project is to provide and API that allows developers to develop in high-level languages, but also to support the development of  high level languages such as sCrypt for smart contracts and the like.

Chi has the luxury of some spare time while we wait for feedback on the first release, to explore issues around the development of a high-level script language, and script programming in general. Note that Steve has previously stated that he would prefer that 3rd parties were responsible for development of such languages so the investigation must be limited (initially assume no longer than a week) 

The following could be part of the investigation. There is not enough time to cover all of these points in depth but all are worth investigation. All are questions that technical users may ask themselves.

- What script languages already exist? What are their characteristics? It is worth looking at BTC, BAB as well as BSV. Do they have dedicated debugging environments?
- What requirements do smart contracts place on a high-level script language?  How are smart contracts typically implemented? In terms of architecture (servers)?
This is a potentially massive topic so don’t go down a rabbit hole. It might make sense to look at the other topics first.
- What would/should a high level script language look like?

Possibly syntax would look like an existing language (Javascript/Python/etc) but not support certain features (inheritance? Unbounded while loops, library support for networking, file systems etc).
One approach is to write code snippets for common constructs (if-then-else, for loops, assignments, “native” and user defined function calls, objects?) and see how they translate into script (pushes, pops etc).
There accumulator multi-sig pattern would also be an interesting target for this exercise. See attached spec. Note this spec if not for distribution.

The above is closely related to the question: what is the “best” way to program script? 
Note that there may be hidden expertise amongst the researchers and developers.

- What features can we add to the script engine library to help support high-level script languages?  E.g. library functions.

The investigation should be a _pen and paper_ affair. E.g. There is probably no reason to attempt to build a prototype.
The result of the investigation could include

- (a) summary of findings re existing products, 
- (b) recommendations on how to build such a high-level script language, 
- (b) general recommendations on how to program script, 
- (c) recommendations on how to support smart contacts, 
- (d) other. Ideally it should be in a form that can be presented to others in this group.

Shaun
```