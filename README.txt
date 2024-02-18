# CS-E5520 Advanced Computer Graphics, Spring 2024
# Lehtinen / Härkönen, Timonen
#
# Assignment 1: Accelerated Ray Tracing

Student name: Huang Yaojun
Student number: 101627324

# Which parts of the assignment did you complete? Mark them 'done'.
# You can also mark non-completed parts as 'attempted' if you spent a fair amount of
# effort on them. If you do, explain the work you did in the problems/bugs section
# and leave your 'attempt' code in place (commented out if necessary) so we can see it.

R1 BVH construction and traversal (5p): done
        R2 BVH saving and loading (1p): done
              R3 Simple texturing (1p): done
             R4 Ambient occlusion (2p): done
         R5 Simple multithreading (1p): done

# Did you do any extra credit work?

(Describe what you did and, if there was a substantial amount of work involved, how you did it. If your extra features are interactive or can be toggled, describe how to use them.)

1. BVH Surface Area Heuristic
	This can be toggled by an added toggle "Use SAH". With SAH, the construction of BVH is about 5 times slower but is 30-40% faster in performing tracing.
	The implemented version of SAH tries to find the optimal split dimension. I also tried to use the largest extent as did in object median. The build time of the optimal split
	is 3 times slower than spilting the dimension with the largest extent, but about 15% faster in tracing. Since the construction result can be stored in disk. So optimal split
	is a better choice.
2. Efficient SAH building
	Compute all AABB area of [start,i] and [i,end], store them into vectors. This reduces redundant computation when finding the optimal split index.
3. specular textures
	This can be open and close by an added toggle "Enable Specular". This is computed bythe Blinn-Phone model (in demo the light direction is the same as the view direction)
4. Tangent space normal mappingreading
	This can be enabled by the toggle "Use normal mapping". First compute the tangent and bitangents to get the TBN, then transform the normal from the normal map.

# Are there any known problems/bugs remaining in your code?

(Please provide a list of the problems. If possible, describe what you think the cause is, how you have attempted to diagnose or fix the problem, and how you would attempt to diagnose or fix it if you had more time or motivation. This is important: we are more likely to assign partial credit if you help us understand what's going on.)

# Did you collaborate with anyone in the class?

(Did you help others? Did others help you? Let us know who you talked to, and what sort of help you gave or received.)
No.

# Any other comments you'd like to share about the assignment or the course so far?

(Was the assignment too long? Too hard? Fun or boring? Did you learn something, or was it a total waste of time? Can we do something differently to help you learn? Please be brutally honest; we won't take it personally.)
The assignment is really great. It practise my programming skills and solidates my understanding. But the code is lack of some description especially in some important
interfaces.

