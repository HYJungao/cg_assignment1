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
	The final version of SAH tries to find the optimal split dimension. I also tried to use the largest extent as I did in the object median method.
	The build time of the optimal splitting is 3 times slower than splitting the dimension to the largest extent method, but about 15% faster in tracing.
	Since the construction result can be stored on disk. So optimal split is a better choice.

2. Efficient SAH building
	Compute all AABB areas of triangle group [start,i] and [i,end] first and then store them into vectors. This reduces redundant computation when finding the optimal split index.

3. specular textures
	This can be open and close by an added toggle "Enable Specular". This is computed bythe Blinn-Phone model (in demo the light direction is the same as the view direction)

4. Tangent space normal mappingreading
	This can be enabled by the toggle "Use normal mapping". First compute the tangent and bitangents to get the TBN, then transform the normal read from the normal map.

5. Bilinear interpolation texture filtering
	This can be turned on by an added toggle "Enable Bilinear Filtering". After getting the texture coordinate of the hit point, we get four neighbours of the coordinate and do interpolation in two dimensions.
	Careful handling of seams by taking modular operation to the coordinates, rather clamping to 0 or image width/height] to make smooth look.
	Applying bilinear interpolation also causes some offset in mapping, fix this by substract 0.5.

# Are there any known problems/bugs remaining in your code?

(Please provide a list of the problems. If possible, describe what you think the cause is, how you have attempted to diagnose or fix the problem, and how you would attempt to diagnose or fix it if you had more time or motivation. This is important: we are more likely to assign partial credit if you help us understand what's going on.)

	After enabling normal mapping, the rendering result has an effect of Moire pattern. This can be improved by mip map or other advanced filtering techniques.
	The texture alpha is also not handled, so the ray tracer does not produce good image especially with plants in crytek-sponza.

# Did you collaborate with anyone in the class?

(Did you help others? Did others help you? Let us know who you talked to, and what sort of help you gave or received.)

	No. But I got inspiration from the slack channel and other forums on the Internet.

# Any other comments you'd like to share about the assignment or the course so far?

(Was the assignment too long? Too hard? Fun or boring? Did you learn something, or was it a total waste of time? Can we do something differently to help you learn? Please be brutally honest; we won't take it personally.)

	The assignment is really great. It practise my programming skills and solidates my understanding on algorithms. It feels great when seeing all things perform well. 
	But the code lacks some description, especially in some important functions and interfaces.

