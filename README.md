# LocalLayering for Gimp

This code implements the Local Layering concept introduced by [2] as a GIMP plugin. 

[Project Page](http://www.cse.iitb.ac.in/~paragc/research/local_layering/index.shtml)  	[Demo Video](https://www.youtube.com/watch?v=loUN9aFjNgg) 

Two versions of the source code for the plugin are released. One uses GIMP parasites to store the additional data-structures necessary for local-layering. The other implements local-layering using image masks as explained in [1].

GIMP 2.6 sources are necessary to compile the plug-in.

The plug-in sources are provided under the GNU General Public License.

[1] Local Manipulation of Image Layers Using Standard Image Processing Primitives, Niranjan Mujumdar, Sanju Maliakal, Sweta Malankar, Satishkumar Chavan, Parag Chaudhuri, Seventh Indian Conference on Computer Vision, Graphics and Image Processing (ICVGIP) 2010. 

[2] Local Layering, James McCann and Nancy Pollard, ACM SIGGRAPH 2009. http://graphics.cs.cmu.edu/projects/local-layering/
