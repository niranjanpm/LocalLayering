
/*
 * This is the Local Layering plug-in for GIMP 2.6 >
 * 
 * Copyright (C) 2009-2010 SNS :)
 * 1. Sanju Maliakal	(sanjumaliakal@gmail.com)
 * 2. Niranjan Mujumdar (niranjanpm@gmail.com)
 * 3. Sweta Malankar	(sweneera@yahoo.com)

 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 */

/* Please read the README for basic instructions on how to install and use the 
 * plugin.
 *
 * Please send any comments or suggestions to us,
 * SNS (locallayering.gimp@gmail.com)
 *
 * Please visit the project website for updates and bug reports.
 * http://sourceforge.net/projects/llgimp/
 */

//required GIMP Libraries
#include <libgimp/gimp.h>
#include <libgimp/gimpui.h>

//for all the memory allocations
#include <stdlib.h>

//for all the string functions
#include <string.h>

//for the power function : pow() ... later discarded
//#include <math.h>

#define PLUG_IN_PROC	"local-layering-5"
#define PLUG_IN_BINARY	"ll"

// Connectivity used in the region growing (extract_tags)
// and bounding rectangle functions 
#define CONNECTIVITY		4

// Maximum number of UNDO operations stored in the UNDO array
// After UNDO_COUNT the initial UNDO's get overwritten
// ie UNDO array implemented as a circular queue
#define UNDO_COUNT		100

// Maximum number of flips stored in the UNDO array's flips 2D array
// After UNDO_FLIP_COUNT the initial flips get overwritten
// ie flips 2D array implemented as a circular queue
#define UNDO_FLIP_COUNT 	1000



static void query (void);
static void run   (const gchar      *name,
                   gint              nparams,
                   const GimpParam  *param,
                   gint             *nreturn_vals,
                   GimpParam       **return_vals);

//Holds the Layer Details
struct ll_layer
{
 gint		id;
 gint		off_x;
 gint		off_y;
 gint		width;
 gint		height;
 gboolean	alpha;
};

//Holds the Layer as a Pixel Region
//along with a flag if the layer is to br considererd for processing
//ie. whether layer intersects with image bounds
struct ll_pr_layer
{
 GimpPixelRgn	layer;
 gboolean	process;
};

//Holds the Layer Mask details
struct ll_layer_mask
{
 gboolean	exists;
 gint		layer_id;
 gint		mask_id;
 gint		off_x;
 gint		off_y;
 gint		width;
 gint		height;
 //gboolean	alpha;
};

//Holds the Layer Mask as a Pixel Region
//along with a flag if the mask is to br considererd for processing
//ie. whether its corresponding layer intersects with image bounds
struct ll_pr_mask
{
 GimpPixelRgn	mask;
 gboolean	process;
};

typedef struct ll_layer		LL_LAYER;

typedef struct ll_pr_layer	LL_PR_LAYER;

typedef struct ll_layer_mask	LL_MASK;

typedef struct ll_pr_mask	LL_PR_MASK;

//Holds values of Cursor within the Plug-in Preview
typedef struct
{
  gint     posx;
  gint     posy;
} LLCursorValues;

//Holds the details of the Cursor within the Plug-in Preview
typedef struct
{
  GimpDrawable *drawable;
  GimpPreview  *preview;
  GtkWidget    *coords;
  gboolean      cursor_drawn;
  gint          curx, cury;                 /* x,y of cursor in preview */
  gint          oldx, oldy;
} LLCursorCenter;


//Simple queue data structure
typedef struct ll_queue
{
	gint * data;
	gint front;
	gint back;
}LL_QUEUE;

//Offsets for Region Growing ie. extract_tags function
typedef struct ll_ofs
{
	gint x;
	gint y;
}LL_OFS;

//Data structure for the List Graph
//Holds the details of the local stacking of layers at all regions
//as well as the connected regions to maintain consistency
typedef struct list_graph
{
	gint ** lists;
	gint ** edges;
}LIST_GRAPH;

//Holds the UNDO information
struct ll_undo_array
{
 gint		call_type;
 gint		call_count;
 gint		flips[UNDO_FLIP_COUNT][3]; 
};

typedef struct ll_undo_array	LL_UNDO_ARRAY;


//Holds the Initial Cursor values
static LLCursorValues llvals =
{
  0, 0  /* posx, posy */
};

static void		layer_code_mem_alloc();

static void		extract_layer_code();

static void		layer_mem_alloc();

static void		layer_details();

static void		pr_details();

static void		mask_mem_alloc();

static void		create_masks_details();

static void		pr_mask_details();

static void		init_ll_map ();

static gboolean		LL_dialog                   ();

static GtkWidget * 	LL_center_create            (GimpDrawable	*drawable,
						     GimpPreview	*preview);
static void		LL_center_cursor_draw       (LLCursorCenter	*center);
static void		LL_center_coords_update     (GimpSizeEntry	*coords,
                                                     LLCursorCenter	*center);
static void		LL_center_cursor_update     (LLCursorCenter	*center);
static void		LL_center_preview_realize   (GtkWidget		*widget,
                                                     LLCursorCenter	*center);
static gboolean		LL_center_preview_expose    (GtkWidget		*widget,
                                                     GdkEvent		*event,
                                                     LLCursorCenter	*center);
static gboolean		LL_center_preview_events    (GtkWidget		*widget,
                                                     GdkEvent		*event,
                                                     LLCursorCenter	*center);

static void 		queue_init(LL_QUEUE * queue_l);

static gboolean 	queue_isempty(LL_QUEUE * queue_l);

static void 		queue_push(LL_QUEUE * queue_l, gint n);

static gint 		queue_pop(LL_QUEUE * queue_l);

static gint 		queue_fetch(LL_QUEUE * queue_l);

static void 		graph_mem_alloc();

static void 		graph_lists_init();	

static void 		graph_edges_init();

static void		tags_mem_alloc();

static void 		extract_tags();

static void 		mask_set_pixel();

static void 		add_masks();

void 			flip_up(gint i1, gint i2, gint l);

void 			flip_down(gint i1, gint i2, gint l);

static void 		get_image_pos();

static void 		print_layer_code();

static void 		print_tags();

static void 		print_graph_lists();

static void 		print_graph_edges();

static void 		reg_affected_malloc();

static void 		clear_reg_affected();

static void 		set_reg_affected();

static void 		init_flip_dialog();

static void 		create_flip_dialog();

static void 		call_flip_up();

static void 		call_flip_down();

static void 		destroy_masks();

static gboolean 	ll_parasite_attach();

static gboolean 	ll_parasite_exists();

static void		ll_parasite_recover();

static void 		ll_parasite_detach();

static void 		rg_boundary_rect();

static void 		rg_boundary_rect_regions();

static void 		update_preview();

static void 		rem_add_prev_drawable();

static void 		init_undo();

static void 		flip_undo();

static void		print_warning();


gint			image_id;
gint			image_height, image_width;
gint			*layers, layer_num;
//gboolean		***layer_present; 
gint			**layer_code;
LL_LAYER		*layer;
LL_PR_LAYER		*pr;
LL_MASK			*mask;
LL_PR_MASK		*pr_mask;
static gboolean   	show_cursor = TRUE;
gint			*tags, *old_tags;
gint			num_regions;
LIST_GRAPH		*graph;
gboolean		restart_LL;
gboolean		*reg_affected;	
gint 			*rg_boundary;
gint			rg_boundary_call;
gint 			pos_x, pos_y;
GtkWidget   		*dialog;
GtkWidget		*main_vbox;
GtkWidget		*preview;
GtkWidget		*frame;
GimpDrawable		*drawable_preview;
gint			drawable_preview_id;
GtkWidget   		*flip_dialog;
GtkWidget 		*flip_vbox;
GtkWidget 		*LL_hbox;
GtkWidget 		*LL_vbox;
GtkWidget 		*lframe;
GtkWidget		*b_flip_up, *b_flip_down, *b_flip_undo, *b_box;
GtkWidget 		**radio;
GtkWidget 		**r_label, **l_label;
GtkWidget 		**r_hbox;
GtkWidget 		**l_button;
gint			*sorted_layer_index;
LL_UNDO_ARRAY		undo_array[UNDO_COUNT];
gint			undo_index, undo_check;


GimpPlugInInfo PLUG_IN_INFO =
{
  NULL,
  NULL,
  query,
  run
};


MAIN()

static void
query (void)
{
	static GimpParamDef args[] = 
	{    
		{ GIMP_PDB_INT32,	"run-mode", 	"Interactive"},
	    	{ GIMP_PDB_IMAGE,	"image", 	"Input image"},
	    	{ GIMP_PDB_DRAWABLE, 	"drawable", 	"Input drawable"},
		{ GIMP_PDB_INT32,    	"pos-x",    	"X-position"},
    		{ GIMP_PDB_INT32,    	"pos-y",    	"Y-position"}
	};

	gimp_install_procedure
	(
		//Register the plugin in the PDB
		"local-layering-5",
		"Local Layering 5",
		"Local Layering 5",
		"SNS",
		"Copyright SNS",
		"2009",
		"_local-layering-5",
		"RGB*, GRAY*",
		GIMP_PLUGIN,
    		G_N_ELEMENTS (args), 0,
    		args, NULL
	);

	// Register menu entry for plugin
	gimp_plugin_menu_register("local-layering-5","<Image>/Filters/Misc");

}

static void
run (const gchar      *name,
     gint              nparams,
     const GimpParam  *param,
     gint             *nreturn_vals,
     GimpParam       **return_vals)
{
	static GimpParam	values[1];
  	GimpPDBStatusType	status = GIMP_PDB_SUCCESS;
  	GimpRunMode		run_mode;
	gint			k;


	run_mode = param[0].data.d_int32;	
  	image_id = param[1].data.d_int32;

	*nreturn_vals = 1;
	*return_vals  = values;

	values[0].type = GIMP_PDB_STATUS;
	values[0].data.d_status = status;


	if (run_mode != GIMP_RUN_NONINTERACTIVE)
	{

		gimp_get_data (PLUG_IN_PROC, &llvals);

		gimp_progress_init (("Local Layering Init"));

		//layers = gimp_image_get_layers (image_id, &layer_num);

		//gimp_tile_cache_ntiles (layer_num * 2 * (gimp_image_width(image_id) / gimp_tile_width () + 1) * (gimp_image_height (image_id) / gimp_tile_height () + 1));

		//g_printf("\n\nntiles = %d",layer_num * 2 * (gimp_image_width(image_id) / gimp_tile_width () + 1) * (gimp_image_height (image_id) / gimp_tile_height () + 1));


		if (! LL_dialog ())
        	{
			if(gimp_drawable_is_valid(drawable_preview_id)) 
				gimp_image_remove_layer (image_id, drawable_preview_id);

			//CANCEL OR CLOSE BUTTON CLICKED

			if(ll_parasite_exists())
			{
				if(!restart_LL)
				{
					ll_parasite_recover();

					set_reg_affected();

					mask_set_pixel();
				}
				else
				{
					destroy_masks();
				}
			}
			else
			{
				destroy_masks();
			}

			gimp_selection_none (image_id);

			gimp_displays_flush();

          		return;
        	}
	}

	//OK BUTTON CLICKED

	
//	if (status == GIMP_PDB_SUCCESS)
//	{

	//FLUSH the drawables (layers and masks) after plugin execution
	if (run_mode != GIMP_RUN_NONINTERACTIVE)
		gimp_displays_flush ();

	//  Store data  
	if (run_mode == GIMP_RUN_INTERACTIVE)
		gimp_set_data (PLUG_IN_PROC, &llvals, sizeof (LLCursorValues));

//	}

//  	values[0].data.d_status = status;
	
	if(gimp_drawable_is_valid(drawable_preview_id)) 
		gimp_image_remove_layer (image_id, drawable_preview_id);

	gimp_selection_none (image_id);

	//gimp_displays_flush();

	if(ll_parasite_exists())
	{
		ll_parasite_detach();
	}

	ll_parasite_attach();



}

//Main Graphical User Interface Dialog
static gboolean 
LL_dialog ()
{
 gboolean     run;

	init_undo();
	init_ll_map();

	gimp_ui_init (PLUG_IN_PROC, TRUE);

	dialog = gimp_dialog_new (("Local Layering"), PLUG_IN_PROC,
		                      NULL, 0,
		                      gimp_standard_help_func, PLUG_IN_PROC,

		                      GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
		                      GTK_STOCK_OK,     GTK_RESPONSE_OK,

		                      NULL);

	gtk_dialog_set_alternative_button_order (GTK_DIALOG (dialog),
		                                     GTK_RESPONSE_OK,
		                                     GTK_RESPONSE_CANCEL,
		                                     -1);

	gimp_window_set_transient (GTK_WINDOW (dialog));

	LL_vbox = gtk_vbox_new (FALSE, 30);
	gtk_container_set_border_width (GTK_CONTAINER (LL_vbox), 12);
	gtk_container_add (GTK_CONTAINER (GTK_DIALOG (dialog)->vbox), LL_vbox);
	gtk_widget_show (LL_vbox);
	
	LL_hbox = gtk_hbox_new (FALSE, 30);	
	gtk_box_pack_start (GTK_BOX (LL_vbox), LL_hbox, TRUE, TRUE, 0);
	gtk_widget_show (LL_hbox);
	
	main_vbox = gtk_vbox_new (FALSE, 12);
	//gtk_container_set_border_width (GTK_CONTAINER (main_vbox), 12);
	//gtk_container_add (GTK_CONTAINER (GTK_DIALOG (dialog)->vbox), main_vbox);
	gtk_box_pack_start (GTK_BOX (LL_hbox), main_vbox, TRUE, TRUE, 0);
	gtk_widget_show (main_vbox);

	rg_boundary_call = 1;

	update_preview();

	gtk_widget_show (dialog);

	run = (gimp_dialog_run (GIMP_DIALOG (dialog)) == GTK_RESPONSE_OK);
	gtk_widget_destroy (dialog);

  	return run;
}

//Sub Dialog which shows the FLip Up , Flip Down and UNDO Buttons
//as well as the local stacking of layers at a point in the preview
//using raido buttons and thumbnails of layers within buttons
static void create_flip_dialog()
{	

	if(flip_vbox != NULL)
	{
		gtk_widget_destroy(flip_vbox);
	}

	flip_vbox = gtk_vbox_new (FALSE, 12);
	//gtk_container_set_border_width (GTK_CONTAINER (flip_vbox), 12);
	//gtk_container_add (GTK_CONTAINER (GTK_DIALOG (flip_dialog)->vbox), flip_vbox);
	gtk_box_pack_start (GTK_BOX (LL_hbox), flip_vbox, TRUE, TRUE, 0);
	
	lframe = gimp_frame_new("LAYERS");
	b_box = gtk_hbox_new(FALSE,12);
	b_flip_up = gtk_button_new_with_label("UP");
	b_flip_down = gtk_button_new_with_label("DOWN");
	b_flip_undo = gtk_button_new_with_label("UNDO");
	gtk_box_pack_start (GTK_BOX (flip_vbox), lframe, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (flip_vbox), b_box , TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (b_box), b_flip_up, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (b_box), b_flip_down, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (b_box), b_flip_undo, TRUE, TRUE, 0);

	gtk_widget_show (flip_vbox);
	gtk_widget_show (lframe);
	gtk_widget_show (b_box);
	gtk_widget_show (b_flip_up);
	gtk_widget_show (b_flip_down);
	gtk_widget_show (b_flip_undo);

	init_flip_dialog();

}

//Initialization for the Flip buttons and UNDO button dialog
//adds signals to the buttons for appropriate callback functions
//as well as calculation of local stacking of layers at a point in the preview
static void init_flip_dialog()
{
	gint i, j, rg_tag, l;
	gint min, min_index;
	gchar *buf;

	//g_printf("\n\nINIT_FLIP_DIALOG\n");

	get_image_pos();

	rg_tag = tags[pos_y * image_width + pos_x];


	l_label		= (GtkWidget **) malloc (layer_num * sizeof(GtkWidget *) );
	r_label		= (GtkWidget **) malloc (layer_num * sizeof(GtkWidget *) );
	r_hbox		= (GtkWidget **) malloc (layer_num * sizeof(GtkWidget *) );
	l_button	= (GtkWidget **) malloc (layer_num * sizeof(GtkWidget *) );
	radio		= (GtkWidget **) malloc (layer_num * sizeof(GtkWidget *) );
	for(i = 0; i < layer_num; i++)
	{	
		l_label[i]	= (GtkWidget *) malloc (layer_num * sizeof(GtkWidget **) );
		r_label[i]	= (GtkWidget *) malloc (layer_num * sizeof(GtkWidget **) );
  		r_hbox[i]	= (GtkWidget *) malloc (layer_num * sizeof(GtkWidget ** ) );
		l_button[i]	= (GtkWidget *) malloc (layer_num * sizeof(GtkWidget **) );
		radio[i]	= (GtkWidget *) malloc (layer_num * sizeof(GtkWidget **) );
	}

	buf = g_new(gchar,4);

	sorted_layer_index = (gint *)malloc(layer_num * sizeof(gint));

	for(i = 0; i < layer_num; i++)
	{
		sorted_layer_index[i] = -1;
	}

	for(i = 0; i < layer_num; i++)
	{
		for(j = 0; j < layer_num; j++)
		{
			if(graph->lists[rg_tag-1][j] == i+1)
			{
				sorted_layer_index[i] = j;
				break;
			}
		}
	}

	radio[0] = gtk_radio_button_new(NULL);

	l = 0;
	for(i = 0; i < layer_num; i++)
	{
		if(sorted_layer_index[i] != -1 )
		{
			l++;
			r_hbox[i] = gtk_hbox_new (TRUE, 12);
			gtk_box_pack_start (GTK_BOX (flip_vbox), r_hbox[i], TRUE, TRUE, 0);
			gtk_widget_show(r_hbox[i]);

			if(i != 0)
				radio[i] = gtk_radio_button_new_from_widget (GTK_RADIO_BUTTON (radio[0]));
				
			if(l == 1)
				gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio[i]), TRUE);
			gtk_box_pack_start (GTK_BOX (r_hbox[i]), radio[i], TRUE, TRUE, 0);
			gtk_widget_show(radio[i]);

				
			l_label[i] = gtk_label_new (NULL);
			gtk_label_set_text(GTK_LABEL (l_label[i]),"LAYER ");
			gtk_box_pack_start (GTK_BOX (r_hbox[i]), l_label[i], TRUE, TRUE, 0);
			gtk_widget_show(l_label[i]);

			r_label[i] = gtk_label_new(NULL);

			g_ascii_dtostr (buf, 4, sorted_layer_index[i] );
			//g_printf("\n Buf = %c",buf[0]);
			gtk_label_set_text(GTK_LABEL (r_label[i]),buf);		
			gtk_box_pack_start (GTK_BOX (r_hbox[i]), r_label[i], TRUE, TRUE, 0);		
			gtk_widget_show(r_label[i]);			

			l_button[i] = gtk_toggle_button_new();
			gtk_button_set_image(GTK_BUTTON(l_button[i]), gtk_image_new_from_pixbuf(gimp_drawable_get_thumbnail (layer[sorted_layer_index[i]].id, 50,50,GIMP_PIXBUF_KEEP_ALPHA)) );
			gtk_box_pack_start (GTK_BOX (r_hbox[i]), l_button[i], TRUE, TRUE, 0);
			gtk_widget_show(l_button[i]);

		}

	}

	g_signal_connect(GTK_OBJECT(b_flip_up), "clicked", G_CALLBACK(call_flip_up), NULL);

	g_signal_connect(GTK_OBJECT(b_flip_up), "clicked", G_CALLBACK(create_flip_dialog), NULL);

	g_signal_connect(GTK_OBJECT(b_flip_down), "clicked", G_CALLBACK(call_flip_down), NULL);

	g_signal_connect(GTK_OBJECT(b_flip_down), "clicked", G_CALLBACK(create_flip_dialog), NULL);

	g_signal_connect(GTK_OBJECT(b_flip_undo), "clicked", G_CALLBACK(flip_undo), NULL);

	g_signal_connect(GTK_OBJECT(b_flip_undo), "clicked", G_CALLBACK(create_flip_dialog), NULL);

}	

//Removes the existing Preview Drawable
//and adds an updated one after a flip or undo call
static void rem_add_prev_drawable()
{
	gint dup_image_id;

	gimp_selection_none (image_id);

	if(gimp_drawable_is_valid(drawable_preview_id))
	{
		gimp_image_remove_layer (image_id, drawable_preview_id);
	}

	//gimp_layer_new_from_visible : does not give the updated visible region after the flip or undo call
	//on the current image. (Some unsorted issue with clearing of buffers)
	//hence a bit of round about code to give correct preview from visible regions

	dup_image_id = gimp_image_duplicate (image_id);

	//drawable_preview_id = gimp_image_merge_visible_layers (dup_image_id, GIMP_CLIP_TO_IMAGE);

	drawable_preview_id = gimp_layer_new_from_visible (dup_image_id, image_id,"DRAW_PREVIEW");

	gimp_image_add_layer (image_id, drawable_preview_id, layer_num);

	gimp_drawable_set_visible (drawable_preview_id,FALSE);
}

//Updates the preview after a flip or undo call
static void update_preview()
{	
	rem_add_prev_drawable();

	if(preview != NULL)
		gtk_widget_destroy(preview);
	if(frame != NULL)
		gtk_widget_destroy(frame);

	drawable_preview = gimp_drawable_get (drawable_preview_id);

	preview = gimp_zoom_preview_new (drawable_preview);

	gtk_widget_add_events (GIMP_PREVIEW (preview)->area, GDK_POINTER_MOTION_MASK);
	gtk_box_pack_start (GTK_BOX (main_vbox), preview, TRUE, TRUE, 0);
	gtk_widget_show (preview);
	
	g_signal_connect_swapped (preview, "invalidated",G_CALLBACK (gtk_widget_show), preview);
	g_signal_connect_swapped (preview, "invalidated",G_CALLBACK (create_flip_dialog), NULL);
	g_signal_connect_swapped (preview, "invalidated",G_CALLBACK (rg_boundary_rect_regions),NULL);

	frame = LL_center_create (drawable_preview, GIMP_PREVIEW (preview));
	gtk_box_pack_start (GTK_BOX (main_vbox), frame, FALSE, FALSE, 0);
	gtk_widget_show (frame);

}

//Prepares the parameters from the Flip Dialog
//and passes it to the flip_up function
//as well as stores value in the UNDO array
static void call_flip_up()
{	
	gint i, l_index, h_index, rg_tag;
	gint k;

		clear_reg_affected();

		undo_check = 1;

		get_image_pos();
		rg_tag = tags[pos_y * image_width + pos_x];	

		k = 0;
	   	for(i = 0; i < layer_num; i++)
		{
			if ( sorted_layer_index[i] != -1)
			{
				k++;
				if(gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (radio[i])))
				{
					h_index = sorted_layer_index[i];
					break;
				}
			}
		}

		if(!(k == 0 || k == 1))
		{

			if( undo_index == UNDO_COUNT-1)
				undo_index = 0;
			else
				undo_index++;

			undo_array[undo_index].call_count++;

			undo_array[undo_index].call_type = 0;

			l_index = sorted_layer_index[i-1];
	
			undo_array[undo_index].flips[undo_array[undo_index].call_count][1] = h_index;
			undo_array[undo_index].flips[undo_array[undo_index].call_count][0] = l_index;
			undo_array[undo_index].flips[undo_array[undo_index].call_count][2] = rg_tag-1;

			flip_up(h_index, l_index, rg_tag-1);

			mask_set_pixel();

			gimp_displays_flush ();

			rg_boundary_call = 0;

			update_preview();

		}

}

//Prepares the parameters from the Flip Dialog
//and passes it to the flip_down function
//as well as stores value in the UNDO array
static void call_flip_down()
{	
	gint i, l_index, h_index, rg_tag;
	gint k;

		clear_reg_affected();

		undo_check = 1;

		get_image_pos();
		rg_tag = tags[pos_y * image_width + pos_x];	

		k = 0;
   		for(i = layer_num-1; i >= 0; i--)
		{
			if ( sorted_layer_index[i] != -1)
			{
				k++;
				if(gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (radio[i])))
				{
					l_index = sorted_layer_index[i];
					break;
				}
			}
		}


		if(!(k == 0 || k == 1 ))
		{

			if( undo_index == UNDO_COUNT-1)
				undo_index = 0;
			else
				undo_index++;

			undo_array[undo_index].call_count++;

			undo_array[undo_index].call_type = 1;	

			h_index = sorted_layer_index[i+1];
	
			undo_array[undo_index].flips[undo_array[undo_index].call_count][1] = h_index;
			undo_array[undo_index].flips[undo_array[undo_index].call_count][0] = l_index;
			undo_array[undo_index].flips[undo_array[undo_index].call_count][2] = rg_tag-1;


			flip_down(l_index, h_index, rg_tag-1);

			mask_set_pixel();

			gimp_displays_flush ();

			rg_boundary_call = 0;

			update_preview();

		}
	
}

//Initializes the UNDO array
static void init_undo()
{
	gint i, j;
	
	undo_index = -1;
	undo_check = 0;

	for(i = 0; i < UNDO_COUNT; i++)
	{
		undo_array[i].call_type = 0;
		undo_array[i].call_count = -1;
		for(j = 0; j < UNDO_FLIP_COUNT; j++)
		{
			undo_array[i].flips[j][0] = -1;
			undo_array[i].flips[j][1] = -1;
			undo_array[i].flips[j][2] = -1;
		}
	}
}

//UNDO function to undo the previous flip / flips
static void flip_undo()
{
	gint i, j;

	if(undo_index == -1)// && undo_array[undo_index].call_count = 0;) //check
	{
		g_printf("\nNO UNDO DATA IN ARRAY\n");
	}
	else 
	{

		undo_check = 0;

		clear_reg_affected();

		for(i = undo_array[undo_index].call_count; i >= 0; i--)
		//for(i = 0; i <= undo_array[undo_index].call_count; i++)
		{

			if((undo_array[undo_index].flips[i][0] != -1) && (undo_array[undo_index].flips[i][1] != -1) && (undo_array[undo_index].flips[i][2] != -1) )
			{		
				if(undo_array[undo_index].call_type == 1)	
				{
					flip_up( undo_array[undo_index].flips[i][0], undo_array[undo_index].flips[i][1], undo_array[undo_index].flips[i][2] );
				}
				else
				{
					flip_down( undo_array[undo_index].flips[i][1], undo_array[undo_index].flips[i][0], undo_array[undo_index].flips[i][2] );
				}
			}

		}


		for(j = 0; j <= undo_array[undo_index].call_count; j++)
		{
			undo_array[undo_index].flips[j][0] = -1;
			undo_array[undo_index].flips[j][1] = -1;
			undo_array[undo_index].flips[j][2] = -1;
		}

		undo_array[undo_index].call_count = -1;	
		undo_index--;

		mask_set_pixel();

		gimp_displays_flush ();

		rg_boundary_call = 0;

		update_preview();

	}

}

//Prepares center of coordinates for Cursor in Preview
static GtkWidget *
LL_center_create (GimpDrawable *drawable,
                  GimpPreview  *preview)
{
  LLCursorCenter *center;
  GtkWidget   *frame;
  GtkWidget   *hbox;
  GtkWidget   *check;
  gint32       image_ID_local;
  gdouble      res_x;
  gdouble      res_y;

  center = g_new0 (LLCursorCenter, 1);

  center->drawable     = drawable;
  center->preview      = preview;
  center->cursor_drawn = FALSE;
  center->curx         = 0;
  center->cury         = 0;
  center->oldx         = 0;
  center->oldy         = 0;

  frame = gimp_frame_new (("LL Cursor"));

  g_object_set_data (G_OBJECT (frame), "center", center);

  g_signal_connect_swapped (frame, "destroy",
                            G_CALLBACK (g_free),
                            center);

  hbox = gtk_hbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER (frame), hbox);
  gtk_widget_show (hbox);

  image_ID_local = gimp_drawable_get_image (drawable->drawable_id);
  gimp_image_get_resolution (image_ID_local, &res_x, &res_y);

  center->coords = gimp_coordinates_new (GIMP_UNIT_PIXEL, "%p", TRUE, TRUE, -1,
                                         GIMP_SIZE_ENTRY_UPDATE_SIZE,
                                         FALSE, FALSE,

                                         ("_X:"), llvals.posx, res_x,
                                         - (gdouble) drawable->width,
                                         2 * drawable->width,
                                         0, drawable->width,

                                         ("_Y:"), llvals.posy, res_y,
                                         - (gdouble) drawable->height,
                                         2 * drawable->height,
                                         0, drawable->height);

  gtk_table_set_row_spacing (GTK_TABLE (center->coords), 1, 12);
  gtk_box_pack_start (GTK_BOX (hbox), center->coords, FALSE, FALSE, 0);
  gtk_widget_show (center->coords);

  g_signal_connect (center->coords, "value-changed",
                    G_CALLBACK (LL_center_coords_update),
                    center);
  g_signal_connect (center->coords, "refval-changed",
                    G_CALLBACK (LL_center_coords_update),
                    center);

  check = gtk_check_button_new_with_mnemonic (("Show _position"));
  gtk_table_attach (GTK_TABLE (center->coords), check, 0, 5, 2, 3,
                    GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check), show_cursor);
  gtk_widget_show (check);

  g_signal_connect (check, "toggled",
                    G_CALLBACK (gimp_toggle_button_update),
                    &show_cursor);
  g_signal_connect_swapped (check, "toggled",
                            G_CALLBACK (gimp_preview_invalidate),
                            preview);

  g_signal_connect (preview->area, "realize",
                    G_CALLBACK (LL_center_preview_realize),
                    center);
  g_signal_connect_after (preview->area, "expose-event",
                          G_CALLBACK (LL_center_preview_expose),
                          center);
  g_signal_connect (preview->area, "event",
                    G_CALLBACK (LL_center_preview_events),
                    center);

  LL_center_cursor_update (center);

  return frame;
}

//Draws the center of coordinates of the Curor in the Preview
static void
LL_center_cursor_draw (LLCursorCenter *center)
{
  GtkWidget *prvw  = center->preview->area;
  GtkStyle  *style = gtk_widget_get_style (prvw);
  gint       width, height;

  gimp_preview_get_size (center->preview, &width, &height);

  gdk_gc_set_function (style->black_gc, GDK_INVERT);

  if (show_cursor)
    {
      if (center->cursor_drawn)
        {
          gdk_draw_line (prvw->window,
                         style->black_gc,
                         center->oldx, 1,
                         center->oldx,
                         height - 1);
          gdk_draw_line (prvw->window,
                         style->black_gc,
                         1, center->oldy,
                         width - 1,
                         center->oldy);
        }

      gdk_draw_line (prvw->window,
                     style->black_gc,
                     center->curx, 1,
                     center->curx,
                     height - 1);
      gdk_draw_line (prvw->window,
                     style->black_gc,
                     1, center->cury,
                     width - 1,
                     center->cury);
    }

  /* current position of cursor is updated */
  center->oldx         = center->curx;
  center->oldy         = center->cury;
  center->cursor_drawn = TRUE;

  gdk_gc_set_function (style->black_gc, GDK_COPY);
}


//Updates the value of the Coordinates of Cursor  in the Preview
static void
LL_center_coords_update (GimpSizeEntry *coords,
                         LLCursorCenter   *center)
{
  llvals.posx = gimp_size_entry_get_refval (coords, 0);
  llvals.posy = gimp_size_entry_get_refval (coords, 1);

  LL_center_cursor_update (center);
  LL_center_cursor_draw (center);

  gimp_preview_invalidate (center->preview);
}


//Redraws the Cursor in the Preview with the updated coordinates
static void
LL_center_cursor_update (LLCursorCenter *center)
{
  gimp_preview_transform (center->preview,
                          llvals.posx, llvals.posy,
                          &center->curx, &center->cury);
}

static void
LL_center_preview_realize (GtkWidget   *widget,
                           LLCursorCenter *center)
{
  GdkDisplay *display = gtk_widget_get_display (widget);
  GdkCursor  *cursor  = gdk_cursor_new_for_display (display, GDK_CROSSHAIR);

  gimp_preview_set_default_cursor (center->preview, cursor);
  gdk_cursor_unref (cursor);
}

/*
 *    Handle the expose event on the preview
 */
static gboolean
LL_center_preview_expose (GtkWidget   *widget,
                          GdkEvent    *event,
                          LLCursorCenter *center)
{
  center->cursor_drawn = FALSE;

  LL_center_cursor_update (center);
  LL_center_cursor_draw (center);

  return FALSE;
}

/*
 *    Handle other events on the preview
 */
static gboolean
LL_center_preview_events (GtkWidget   *widget,
                          GdkEvent    *event,
                          LLCursorCenter *center)
{
	gint tx, ty;

	switch (event->type)
	{
		case GDK_MOTION_NOTIFY:
   			if (! (((GdkEventMotion *) event)->state & GDK_BUTTON1_MASK))
     			break;

		case GDK_BUTTON_PRESS:
      			{
      				GdkEventButton *bevent = (GdkEventButton *) event;

        			if (bevent->button == 1)
       				{
      	   				gimp_preview_untransform (center->preview,bevent->x, bevent->y,&tx, &ty);

            				LL_center_cursor_draw (center);

				g_signal_handlers_block_by_func (center->coords, LL_center_coords_update, center);

					gimp_size_entry_set_refval (GIMP_SIZE_ENTRY (center->coords), 0, tx);

					gimp_size_entry_set_refval (GIMP_SIZE_ENTRY (center->coords), 1, ty);

				g_signal_handlers_unblock_by_func (center->coords, LL_center_coords_update, center);

					LL_center_coords_update (GIMP_SIZE_ENTRY (center->coords), center);
				}
			}
			break;
	
		default:
      			break;
	}

  	return FALSE;
}


//Initialization (memory allocation + calculaion of values) of all the data structures of the plug-in
//ie. layers, masks, tags, list_graph,
//Calls a host of other functions
static void init_ll_map()
{
 
	// Get list of layers and no. of layers for image
	layers = gimp_image_get_layers (image_id, &layer_num);

	image_height = gimp_image_height(image_id);		
	image_width  = gimp_image_width(image_id);

	layer_code_mem_alloc();
		
	layer_mem_alloc();

	mask_mem_alloc();

	extract_layer_code();

	tags_mem_alloc();

	extract_tags();
}

//Memory allocation for the layer_code array
static void layer_code_mem_alloc()
{
 gint	m, n;

	// Dynamic memory allocation for pointer to pointer to pointer to gboolean
	// Holds the Bitfield Pixel Map representing if a Layer exists at a particular Image Location
	/*
	layer_present = (gboolean ***)malloc(image_height * sizeof(*layer_present));
	for(m = 0; m < image_height; m++)
	{
		layer_present[m] = (gboolean **)malloc(image_width * sizeof(**layer_present));
         
		for(n = 0; n < image_width; n++)
		{
			layer_present[m][n] = (gboolean *)malloc(layer_num * sizeof(***layer_present));
		}    
	}
	*/	
	// Dynamic memory allocation for pointer to pointer to int
	// Holds a Pixel Code which represents the layer ordering at that Pixel Location
	layer_code = (gint **)malloc(image_height * sizeof(*layer_code));
	for(m = 0; m < image_height; m++)
	{
		layer_code[m] = (gint *)malloc(image_width * sizeof(**layer_code));               
	}
}

//Memory allocation for the layer and pr arrays
static void layer_mem_alloc()
{
		//Dynamic memory allocation for array of layer's details
		layer = (LL_LAYER *)malloc(layer_num * sizeof(LL_LAYER));

		//Dynamic memory allocation for array of layers as pixel regions
		pr = (LL_PR_LAYER *)malloc(layer_num * sizeof(LL_PR_LAYER));
}

//Initializes the values of the layer array
static void layer_details()
{
 	gint	i;
	gboolean test;

	for(i = 0; i < layer_num; i++)
	{

		// Store layer id
		(layer[i]).id = *(layers + i);

		// Get offset values for all layers
		test = gimp_drawable_offsets(*(layers+i), &((layer[i]).off_x), &((layer[i]).off_y));

		// Get layer width and height
		(layer[i]).width = gimp_drawable_width(*(layers+i));
		(layer[i]).height = gimp_drawable_height(*(layers+i));

		// Get layer alpha
		(layer[i]).alpha = gimp_drawable_has_alpha(*(layers + i));

		//If layer does not have an alpha channel add one	
		if(!((layer[i]).alpha))
		{
			gimp_layer_add_alpha (*(layers+i));
			(layer[i]).alpha = TRUE;
		}

	}
}

//Initializes all the layers as pixel regions
static void pr_details()
{
 GimpDrawable	*pr_drawable;
 gint		i;
 gint		rx, ry, rw, rh;


	// Holds a Drawable to be Passed(Initialized) to a Pixel Region
	pr_drawable = (GimpDrawable *)malloc(sizeof(GimpDrawable));

	for(i = 0; i < layer_num; i++)
	{
		// Get pointer to drawable from drawable id
		pr_drawable = gimp_drawable_get(*(layers+i));

		//Check if Layer(Drawable) intersects with Image and get overlapping Rectangle/Region 
		if(gimp_rectangle_intersect(0, 0 , image_width, image_height, (layer[i]).off_x, (layer[i]).off_y, (layer[i]).width, (layer[i]).height, &rx,&ry,&rw,&rh) )
		{

			//Process layer initialized as a pixel region
			(pr[i]).process = 1;

			//If Partial Overlap
			if(rx > (layer[i]).off_x)
			{
				//If Partial Overlap
				if(ry > (layer[i]).off_y)
				{
					gimp_pixel_rgn_init ( &((pr[i]).layer),pr_drawable,(rx - (layer[i]).off_x),(ry - (layer[i]).off_y), rw, rh, FALSE, FALSE);
				}
				else
				{

					gimp_pixel_rgn_init ( &((pr[i]).layer), pr_drawable,(rx - (layer[i]).off_x),0, rw, rh, FALSE, FALSE);
				}
			}
			//If Partial Overlap
			else if(ry > (layer[i]).off_y)
			{
				gimp_pixel_rgn_init ( &((pr[i]).layer),pr_drawable,0,(ry - (layer[i]).off_y), rw, rh, FALSE, FALSE);
			}
			//If Complete Overlap || even Partial
			else
			{
				gimp_pixel_rgn_init ( &((pr[i]).layer), pr_drawable, 0, 0, rw, rh, FALSE, FALSE);
			}
			pr_drawable++;
		}
		//If No overlap then ignore Layer(Drawable) ie. dont include in processing
		else
		{
			(pr[i]).process = 0;
		}
	}
}

//Memory allocation for the mask and pr_mask arrays
static void mask_mem_alloc()
{
		//Dynamic memory allocation for array of mask's details
		mask = (LL_MASK *)malloc(layer_num * sizeof(LL_MASK));

		//Dynamic memory allocation for array of masks as pixel regions
		pr_mask = (LL_PR_MASK *)malloc(layer_num * sizeof(LL_PR_MASK));
}

//Initializes the values of the mask array
static void create_masks_details()
{
 	gint i;

	for(i = 0 ; i < layer_num ; i++)
	{
		//if layer does not already have a mask
		if(gimp_layer_get_mask ((layer[i]).id) == -1)
		{
			(mask[i]).exists = TRUE;
			(mask[i]).mask_id = gimp_layer_create_mask ((layer[i]).id, GIMP_ADD_ALPHA_MASK);
			(mask[i]).layer_id = (layer[i]).id;
			//gimp_drawable_offsets((mask[i]).mask_id,&((mask[i]).off_x),&((mask[i]).off_y));
			gimp_drawable_offsets((mask[i]).layer_id,&((mask[i]).off_x),&((mask[i]).off_y));
			(mask[i]).width = gimp_drawable_width ((mask[i]).mask_id);
			(mask[i]).height = gimp_drawable_height ((mask[i]).mask_id);
		}
		else
		{
			(mask[i]).exists = TRUE;
			(mask[i]).mask_id = gimp_layer_get_mask ((layer[i]).id);
			(mask[i]).layer_id = (layer[i]).id;
			//gimp_drawable_offsets((mask[i]).mask_id,&((mask[i]).off_x),&((mask[i]).off_y));
			gimp_drawable_offsets((mask[i]).layer_id,&((mask[i]).off_x),&((mask[i]).off_y));
			(mask[i]).width = gimp_drawable_width ((mask[i]).mask_id);
			(mask[i]).height = gimp_drawable_height ((mask[i]).mask_id);
		}
	}
}

//Initializes all the masks as pixel regions
static void pr_mask_details()
{
	GimpDrawable	*pr_mask_drawable;
	gint		i;
	gint		rx, ry, rw, rh;


	// Holds a Drawable to be Passed(Initialized) to a Pixel Region
	pr_mask_drawable = (GimpDrawable *)malloc(sizeof(GimpDrawable));

	//g_printf("\n\nEnter PR MASK DETAILS");

	for(i = 0; i < layer_num; i++)
	{
		//masks are initialized as pixel regions for all layers
		
		pr_mask_drawable = gimp_drawable_get( (mask[i]).mask_id );

			//Check if Mask(Drawable) intersects with Image and get overlapping Rectangle/Region 
			//similar to check off layers cause mask offsets are initialized as layer offsets
			if(gimp_rectangle_intersect(0, 0 , image_width, image_height, (mask[i]).off_x, (mask[i]).off_y, (mask[i]).width, (mask[i]).height, &rx,&ry,&rw,&rh) )
			{
				(pr_mask[i]).process = 1;

				//If Partial Overlap
				if(rx > (mask[i]).off_x)
				{
					//If Partial Overlap
					if(ry > (mask[i]).off_y)
					{
						gimp_pixel_rgn_init ( &((pr_mask[i]).mask),pr_mask_drawable,(rx - (mask[i]).off_x),(ry - (mask[i]).off_y), rw, rh, FALSE, FALSE);
					}
					else
					{
						gimp_pixel_rgn_init ( &((pr_mask[i]).mask), pr_mask_drawable,(rx - (mask[i]).off_x),0, rw, rh, FALSE, FALSE);
					}
				}
				//If Partial Overlap
				else if(ry > (mask[i]).off_y)
				{
					gimp_pixel_rgn_init ( &((pr_mask[i]).mask),pr_mask_drawable,0,(ry - (mask[i]).off_y), rw, rh, FALSE, FALSE);
				}
				//If Complete Overlap || even Partial
				else
				{
					gimp_pixel_rgn_init ( &((pr_mask[i]).mask), pr_mask_drawable, 0, 0, rw, rh, FALSE, FALSE);
				}
				pr_mask_drawable++;
			}
			//If No overlap then ignore Layer(Drawable)
			else
			{
				(pr_mask[i]).process = 0;
			}
	}
}

//Calculates the layer code for each pixel in the image space
static void extract_layer_code()
{
	guchar		*pr_buf;
	gint		rx, ry, rw, rh;
	gdouble		l_opacity;
	gint		y_off, x_off;
	gint		m, n, i;
	gint 		x, y;
	gint 		bytes;
	
	//Get Details of all layers
	layer_details();

	//Get layers as pixel regions
	pr_details();

	//need to verify position of this
	//Make Details of all layers
	create_masks_details();

	//Get masks as pixel regions
	pr_mask_details();

	//Adds masks to all layers
	add_masks();

	//Make Pixel Map
	for(i = 0; i < layer_num; i++)
	{
	//If Pixel Region has been initialized for the ith Layer(Drawable)
	//then process Pixel Region
		if( (pr[i]).process == 1)
		{
			//Get Opacity of Pixel Region ie. Layer(Drawable) held by Pixel Region
			l_opacity = gimp_layer_get_opacity( ((pr[i]).layer.drawable)->drawable_id);

			//Get bytes per pixel of Pixel Region (Layer)
			bytes = (pr[i]).layer.bpp;

			//If Opacity is 0 , assume Layer absent at all its Pixel Locations
			if(l_opacity != 0.0)
			{
				//This Maps the Initial Pixel Location of the Pixel Region 
				//to the corresponding Image Pixel Location 

				y_off = (layer[i]).off_y + (pr[i]).layer.y;
				x_off = (layer[i]).off_x + (pr[i]).layer.x;

				//Character Buffer to hold Pixel returned by gimp_pixel_rgn_get_pixel()
				//pr_buf = (gchar *)malloc( (pr_layer[i]).bpp * sizeof(gchar) * pr_layer[i].w );
				//pr_buf = g_new (guchar, (pr[i]).layer.w * bytes);
				pr_buf = g_new (guchar, (pr[i]).layer.w * (pr[i]).layer.h * bytes);

				gimp_pixel_rgn_get_rect( &((pr[i]).layer), pr_buf, 0, 0, (pr[i]).layer.w, (pr[i]).layer.h);

				for (y = 0; y < ( (pr[i]).layer.h ); y++)
				{
					//gimp_pixel_rgn_get_row( &((pr[i]).layer), pr_buf, 0, y, (pr[i]).layer.w);
					
					for (x = 0; x < ( (pr[i]).layer.w ); x++)
					{
						//Returns Pixel at an Offset x,y of the Pixel Region
						//gimp_pixel_rgn_get_pixel (&pr_layer[i], pr_buf, x, y);

						//If Pixel Region ie. Layer(Drawable) held by Pixel Region 
						//has an Alpha Channel
						if((layer[i]).alpha)
						{
							//If Alpha Value at Pixel is Non Zero
							//Layer is Present at that Pixel
							//if(*(pr_buf  +  (x * bytes) + (bytes - 1) ) != 0)
							if(*(pr_buf  +  (y * ( (pr[i]).layer.w ) * bytes) + (x * bytes) + (bytes - 1) ) != 0)
							{
			 					//layer_present[y_off - (pr[i]).layer.y + y][x_off - (pr[i]).layer.x + x][i] = 1;
								layer_code[y_off - (pr[i]).layer.y + y][x_off - (pr[i]).layer.x + x] += 1 << i;
								//layer_code[y_off + y][x_off + x] += 1 << i;

							}
						}
					//If Pixel Region ie. Layer(Drawable) held by Pixel Region does not 
					//have an Alpha Channel assume Layer is Present at that Pixel
						else
						{
		 					//layer_present[y_off + y][x_off + x][i] = 1;
							layer_code[y_off  - (pr[i]).layer.y + y][x_off - (pr[i]).layer.x + x] += 1 << i;
							//layer_code[y_off + y][x_off + x] += 1 << i;
						}
					}

			 	}
			}
		}
	}
}

//Allocates memory for the tags array which holds the tags calculated for the present layers in extract_tags
//Also allocate memory for old_tags which hold the tags from a previous run of local layering from the TAGS Parasite
static void tags_mem_alloc()
{
	tags = (gint *)malloc(image_width * image_height * sizeof(gint));

	old_tags = (gint *)malloc(image_height * image_width * sizeof(gint));
}

//Calculates the tags array for all the pixels in the images space
//Calculates number of regions, List_Graph : Lists and Edges
static void extract_tags()
{
 LL_QUEUE	seeds,seeds1;
 LL_QUEUE	to_expand,to_expand1;
 LL_OFS		ofs[CONNECTIVITY] = {{-1,0},{1,0},{0,-1},{0,1}};
 gint		**tag_adj;
 gint		cur_tag, cur_tag1, *tags1;
 gint		seed, kind;
 gint		seed_temp, r, c;
 gint		n;
 gint		i,j,k,l,s;
 gint		px, py, rg_tag;
 gint		lfm;

	tags1 = (gint *)malloc(image_height * image_width * sizeof(gint));	
	for(i = 0; i < image_height * image_width; i++)
		tags1[i] = 0;

	//INITIALIZATION 1
 	tags1[0] = -1;
 	cur_tag1 = 0;
 	queue_init(&seeds1);
 	queue_push(&seeds1,0);
 	queue_init(&to_expand1);
	
	//REGION GROWING ALGORITHM 1 : To calculate Number of Regions
	while(!queue_isempty(&seeds1))
	{
		//Get a seed from the seeds queue
 		seed = queue_pop(&seeds1);

 		if(tags1[seed] != -1)
		{//seed has been evaluated
 			continue;
		}
		//Get Fresh Tag		
 		cur_tag1++;

		//Assign tag to tags1[seed]
 		tags1[seed] = cur_tag1;

		//Push seed into to_expand1 queue
 		queue_push(&to_expand1,seed);

		//Get Layer code for pixel
		kind = layer_code[seed/image_width][seed%image_width];		
		
		//while to_expand queue is not empty
		while( !queue_isempty(&to_expand1) )
		{
			seed_temp = queue_pop(&to_expand1);
			r = seed_temp / image_width;
			c = seed_temp % image_width;
			
			for(i = 0; i < 4; i++)
			{
				if((r + ofs[i].y >= image_height) || (r + ofs[i].y < 0) )
					continue;
				
				if((c + ofs[i].x >= image_width) || (c + ofs[i].x < 0) )
					continue;
				
				n = (r + ofs[i].y)*image_width + c + ofs[i].x;
				
				if(layer_code[n/image_width][n%image_width] == kind)
				{	
					if(tags1[n] == cur_tag1)
						continue;
					
					tags1[n] = cur_tag1;
					
					queue_push(&to_expand1,n);
				}
				else if (tags1[n] != 0)
				{	
					//if(tags1[n] != -1)
					//{
						//tag_adj[cur_tag-1][tags[n]-1] = 1;
					//}
				}
				else
				{	
					tags1[n] = -1;
					queue_push(&seeds1,n);
				}
			}
		}
	}

	//Store number of regions present in GIMP Image
	num_regions = cur_tag1;

	//INITIALIZATION 2 : MEMORY ALLOCATION
	graph_mem_alloc();

	restart_LL = TRUE;
	if(ll_parasite_exists())
	{
		 //then initialize ListGraph from the GimpParasite
		
		//Dont Restart Local Layering ie.
		//Dont Reinitialize the ListGraph -> Initialize it from the Parasite
		restart_LL = FALSE;

		ll_parasite_recover();
		//details of previous tags array gets stored in old_tags
		//ListGraph gets initialized previously store values
		//Undo array also gets initialized with the previous flips

		//Check for any changes in calculated tags(tags1) and previously stored tags (old_tags)
		for(i = 0; i < image_height; i++)
		{
			for(j = 0; j < image_width; j++)
			{

				//If any changes are made after atttaching the GimpParasite		
				//restart Local Layering
				if(old_tags[i * image_width + j] == tags1[i * image_width + j])
				{
					//continue;
				}
				else
				{
					//Resart Local Layering
					//ie. Reinitialize the ListGraph
					restart_LL = TRUE;
					break;			
				}
			}

			if(restart_LL)
			{
				//Reset the Undo Data
				init_undo();
	
				//Destroy Masks
				destroy_masks();

				//Recreate the Layer Masks
				create_masks_details();

				//Reinitilaize the Layer Masks as Pixel Regions 
				pr_mask_details();

				//Add Layer Masks to the Corresponding Layers
				add_masks();

				break;			
			}

		}

	}

	
	if(restart_LL)
	{
		//REGION GROWING ALGORITHM 2 : To Calculate ListGraph
		
		//Initialize the ListGraph Lists with 0 values
		graph_lists_init();

		tag_adj = (gint **)malloc(num_regions * sizeof(*tag_adj));
		for(i = 0;i < num_regions; i++)
		{
			tag_adj[i] = (gint *)malloc(num_regions * sizeof(**tag_adj));
			for(j = 0; j < cur_tag1; j++)
				tag_adj[i][j] = 0;
		}

	 	// INIT 2
		for(i = 0; i < image_height * image_width; i++)
			tags[i] = 0;

	 	tags[0] = -1;
	 	cur_tag = 0;
 		queue_init(&seeds);
 		queue_push(&seeds,0);
 		queue_init(&to_expand);

		// ALGORITHM 2
		while(!queue_isempty(&seeds))
		{
		
 			seed = queue_pop(&seeds);

 			if(tags[seed] != -1)		
 				continue;
		
 			cur_tag++;			
 			tags[seed] = cur_tag;		
 
 			queue_push(&to_expand,seed);

			kind = layer_code[seed/image_width][seed%image_width];
			s = 1;

			//Calculate layers present at that pixel and put it into ListGraph
			for (l = 0; l < layer_num; l++)
			{
				if (kind & (1 << l))
				{
					graph->lists[cur_tag-1][l] = s;
					s++;
				}
			}
		
			while( !queue_isempty(&to_expand) )
			{
				seed_temp = queue_pop(&to_expand);
				r = seed_temp / image_width;
				c = seed_temp % image_width;
				
				for(i = 0; i < 4; i++)
				{
					if((r + ofs[i].y >= image_height) || (r + ofs[i].y < 0) )
						continue;
				
					if((c + ofs[i].x >= image_width) || (c + ofs[i].x < 0) )
						continue;
				
					n = (r + ofs[i].y)*image_width + c + ofs[i].x;
					
					if(layer_code[n/image_width][n%image_width] == kind)
					{	
						if(tags[n] == cur_tag)
							continue;
						
						tags[n] = cur_tag;
						
						queue_push(&to_expand,n);
						
					}
					else if (tags[n] != 0)
					{	
						if(tags[n] != -1)
						{						
							tag_adj[cur_tag-1][tags[n]-1] = 1;
							tag_adj[tags[n]-1][cur_tag-1] = 1;
						}
					}
					else
					{	
						tags[n] = -1;
						queue_push(&seeds,n);
					}
				}	
			}
		}

		num_regions = cur_tag;

		//Initialize the ListGraph Edges with 0 values
		graph_edges_init();
 		
		for (i = 0; i < num_regions; i++)
 		{
			for (j = 0; j < num_regions; j++) 
			{
				if ( tag_adj[i][j] ) 
				{
					graph->edges[j][i] = 1;
					graph->edges[i][j] = 1;
				}
			}
		}

	}
	else
	{
		//If Parasite has been attached and 

		for(i = 0; i < image_height; i++)
		{
			for(j = 0; j < image_width; j++)
			{
				tags[i * image_width + j] = old_tags[i * image_width + j];
			}
		}
				
	}

	//Allocate memory for the reg_affected array
	reg_affected_malloc();

	//Set all regions to affected for the initial run of mask_set_pixel
	set_reg_affected();

	//Set Pixel values as per ListGraph values and reg_affected array
	mask_set_pixel();

	//Flush the layers and masks
	gimp_displays_flush();

}


//Function which does the Mask Painting
//Sets the pixels based on contents of ListGraph and reg_affected array
//Follows the principle that at any pixel (region) only the top most layer will have a white (fully opaque) value
//and all layers below it will have a black (fully transparent) value
static void mask_set_pixel()
{		
 guchar		**pr_buf;
 gint		*y_off, *x_off;
 gint		m, n, i, k;
 gint 		x, y;
 gint		min, min_index;

	//if(layer_num != 1)
	//{
	y_off = (gint *)malloc(layer_num * sizeof(gint));
	x_off = (gint *)malloc(layer_num * sizeof(gint));

	//pr_buf = g_new (guchar*, layer_num * (pr_mask[i]).mask.w * (pr_mask[i]).mask.h);

	pr_buf = (guchar **)malloc(layer_num * sizeof(*pr_buf));


	for(i = 0; i < layer_num; i++)
	{

		if( (pr_mask[i]).process ) 
		{
		//pr_buf[i] = g_new (guchar, (pr_mask[i]).mask.w * (pr_mask[i]).mask.h);

			pr_buf[i] = (guchar *)malloc((pr_mask[i]).mask.w * (pr_mask[i]).mask.h * sizeof(**pr_buf));

			gimp_pixel_rgn_get_rect( &((pr_mask[i]).mask), pr_buf[i], 0, 0, (pr_mask[i]).mask.w,(pr_mask[i]).mask.h);

			y_off[i] = (mask[i]).off_y + (pr_mask[i]).mask.y;
			x_off[i] = (mask[i]).off_x + (pr_mask[i]).mask.x;

		//g_free(pr_buf);

		}

	}

	for(m = 0; m < image_height; m++)
	{
		for(n = 0; n < image_width; n++)
		{
			k = tags[m * image_width + n] - 1;

			if(reg_affected[k])
			{
				min_index = -1;
				min = layer_num;

				for(i = 0; i < layer_num; i++)
				{
					if(graph->lists[k][i] != 0)
					{
						if(graph->lists[k][i] < min)
						{
							min = graph->lists[k][i];
							min_index = i;
						}
					}
				}


				for(i = 0; i < layer_num; i++)
				{
					if(m >= y_off[i] && n >=x_off[i] && m < (y_off[i] + (pr_mask[i]).mask.h) && n < (x_off[i] + (pr_mask[i]).mask.w))
					{
						if(min_index == i)
						{
							*(*(pr_buf + i)  + ( (m - y_off[i] + (pr_mask[i]).mask.y) * (pr_mask[i]).mask.w) + (n - x_off[i] + (pr_mask[i]).mask.x)) = 255;
							//*(*(pr_buf + i)  + ( (m - y_off[i]) * (pr_mask[i]).mask.w) + (n - x_off[i])) = 255;

						}
						else
						{
							*(*(pr_buf + i)  + ( (m - y_off[i] + (pr_mask[i]).mask.y) * (pr_mask[i]).mask.w) + (n - x_off[i] + (pr_mask[i]).mask.x)) = 0;
							//*(*(pr_buf + i)  + ( (m - y_off[i]) * (pr_mask[i]).mask.w) + (n - x_off[i])) = 0;
						}
					}
				}
			}
		}
	}



	for(i = 0; i < layer_num; i++)
	{
		if( (pr_mask[i]).process ) 
		{
			gimp_pixel_rgn_set_rect ( &((pr_mask[i]).mask), pr_buf[i], 0, 0, (pr_mask[i]).mask.w, (pr_mask[i]).mask.h);

			gimp_drawable_update ((pr_mask[i]).mask.drawable->drawable_id, 0, 0, (pr_mask[i]).mask.w, (pr_mask[i]).mask.h);
		}

	}

	//}
}

//Add the masks which have been initialized as pixel regions to the respective layers
//also checks if layer does not have any pre existing mask
//if pre existing mask is there then it is not required to add it 
//and modifications on it will be reflected
static void add_masks()
{
 gint	i;

	for(i = 0 ; i < layer_num ; i++)
	{
		//if( (mask[i]).exists && gimp_layer_get_mask ((mask[i]).layer_id) == -1 )
		if( (pr_mask[i]).process && gimp_layer_get_mask ((mask[i]).layer_id) == -1 )
		{
			{
				gimp_layer_add_mask ( mask[i].layer_id, (pr_mask[i]).mask.drawable->drawable_id);
			}
		}
	}


}

//Initializes the elements of the queue
//allocates memory for the queue array
static void queue_init(LL_QUEUE * queue_l)
{
	queue_l->front = 0;
	queue_l->back = 0;
	queue_l->data = (gint *)malloc(image_height * image_width * sizeof(gint));
}

//checks whether the queue is empty
static gboolean queue_isempty(LL_QUEUE * queue_l)
{
	if( queue_l->front == queue_l->back)
	{
		//g_printf("QUEUE EMPTY");
		return TRUE;
	}
	return FALSE; 
}

//Pushes the value passed as parameter to the back of the queue
//increments back to point to space for next element
static void queue_push(LL_QUEUE * queue_l, gint n)
{
	queue_l->data[queue_l->back] = n;
	queue_l->back++;
}

//returns the front element from the queue passed as parameter
//increments front to point to a next value in queue
static gint queue_pop(LL_QUEUE * queue_l)
{
 gint	n = -1;
	if(!queue_isempty(queue_l))
	{
		n = queue_l->data[queue_l->front];
		queue_l->front++;
	}
 return n;
}

/*
static gint queue_fetch(LL_QUEUE * queue_l)
{
 gint	n = -1;

	if(!queue_isempty(queue_l))
	{
		n = queue_l->data[queue_l->front];
	}
 return n;
}
*/


//Allocate memory for the ListGraph Lists and Edges
static void graph_mem_alloc()
{
 gint	i;

	graph = (LIST_GRAPH *)malloc(sizeof(LIST_GRAPH));

	graph->lists = (gint **)malloc( num_regions * sizeof(*(graph->lists)));

	for(i = 0; i < num_regions; i++)
	{
		graph->lists[i] = (gint *)malloc(layer_num * sizeof(**(graph->lists)));
	}

	graph->edges = (gint **)malloc(num_regions * sizeof(*(graph->edges)));

	for(i = 0; i < num_regions; i++)
	{
		graph->edges[i] = (gint *)malloc(num_regions * sizeof(**(graph->edges)));
	}

}


//Initialize the ListGraph Lists to 0 values
static void graph_lists_init()
{
 gint	i, j;

	for (i = 0; i < num_regions; i++)
 	{
		for (j = 0; j < layer_num; j++) 
		{
			graph->lists[i][j] = 0;
		}
	}

}


//Initialize the ListGraph Edges to 0 values
static void graph_edges_init()
{
 gint	i, j;

	for (i = 0; i < num_regions; i++)
 	{
		for (j = 0; j < num_regions; j++) 
		{
			graph->edges[i][j] = 0;
		}
	}

}


//Flips the layer i1 over layer i2 in region l
//Takes care of consistency of the layers in the adjacent regions
//with the help of the list graph and recursive calls 
void flip_up(gint i1, gint i2, gint l)
{
 gint		i, i3, temp, l1, l2;
 gboolean 	check_affected = FALSE;


	l1 = graph->lists[l][i1];
	l2 = graph->lists[l][i2];

	for(i = 0; i < layer_num; i++)
	{
		if( l1 == 0  || l2 == 0 )
			return;
	}

	while(graph->lists[l][i1] > graph->lists[l][i2])
	{	
		for(i = 0; i < layer_num; i++)
		{
			if( graph->lists[l][i] == (graph->lists[l][i1]-1) ) // l1 = 1?
				i3 = i;
		}

		temp = graph->lists[l][i3];
		graph->lists[l][i3] = graph->lists[l][i1];
		graph->lists[l][i1] = temp;

		check_affected = TRUE;

		for(i = 0; i < num_regions; i++)
		{
			if(graph->edges[l][i] == 1)
			{

				if(undo_check)
				{
					undo_array[undo_index].call_count++;

					if( undo_array[undo_index].call_count >= UNDO_FLIP_COUNT)
					{
						print_warning();
						exit(0);
					}

					undo_array[undo_index].flips[undo_array[undo_index].call_count][1] = i1;
					undo_array[undo_index].flips[undo_array[undo_index].call_count][0] = i3;
					undo_array[undo_index].flips[undo_array[undo_index].call_count][2] = i;


					flip_up(i1,i3,i);
				}
			}
		}

	}

	if(check_affected == TRUE)
	{
		reg_affected[l] = TRUE;

	}

}


//Flips the layer i1 beanath layer i2 in region l
//Takes care of consistency of the layers in the adjacent regions
//with the help of the list graph and recursive calls 
void flip_down(gint i1, gint i2, gint l)
{
 gint		i, i3, temp, l1, l2;
 gboolean 	check_affected = FALSE;


	l1 = graph->lists[l][i1];
	l2 = graph->lists[l][i2];

	for(i = 0; i < layer_num; i++)
	{
		if( l1 == 0  || l2 == 0 )
			return;
	}

	while(graph->lists[l][i1] < graph->lists[l][i2])
	{	
		for(i = 0; i < layer_num; i++)
		{
			if( graph->lists[l][i] == (graph->lists[l][i1]+1) ) // l1 = 1?
				i3 = i;
		}

		temp = graph->lists[l][i3];
		graph->lists[l][i3] = graph->lists[l][i1];
		graph->lists[l][i1] = temp;

		check_affected = TRUE;

		for(i = 0; i < num_regions; i++)
		{
			if(graph->edges[l][i] == 1)
			{

				if(undo_check)
				{

					undo_array[undo_index].call_count++;

					if( undo_array[undo_index].call_count >= UNDO_FLIP_COUNT)
					{
						print_warning();
						exit(0);
					}

					undo_array[undo_index].flips[undo_array[undo_index].call_count][1] = i3;
					undo_array[undo_index].flips[undo_array[undo_index].call_count][0] = i1;
					undo_array[undo_index].flips[undo_array[undo_index].call_count][2] = i;

					flip_down(i1,i3,i);
				}
			}
		}

	}

	if(check_affected == TRUE)
	{
		reg_affected[l] = TRUE; 
	}

}

//Allocates memory for the reg_affected array
//This array indicates the regions affected due to a Flip Up or Down call
//to maintain consistency in adjacent regions
static void reg_affected_malloc()
{
 gint	i;

	reg_affected = (gboolean*)malloc(num_regions * sizeof(gboolean));

}

//Clears the reg_affected array
static void clear_reg_affected()
{
 gint i;

	for(i = 0; i < num_regions; i++)
	{
		reg_affected[i] = FALSE;
	}
}

//Sets the reg_affected array
static void set_reg_affected()
{
 gint i;

	for(i = 0; i < num_regions; i++)
	{
		reg_affected[i] = TRUE;
	}
}

//Gets the pixel postions frpm the cursor on the preview
static void get_image_pos()
{
	pos_x = llvals.posx;
	pos_y = llvals.posy;

	//if the cursor goes out of bounds the cursor coordinates are rest to (0,0)
	if(pos_y >= image_height || pos_x >= image_width || pos_y < 0 || pos_x < 0)
	{
		g_printf("\n\nCoordinates Out of Image bounds : Resetting to (0,0)\n");
		//g_message("Coordinates Out of Image bounds : Resetting to (0,0)");
		//gimp_message(("Coordinates Out of Image bounds : Resetting to (0,0)"));
		pos_x = 0;
		pos_y = 0;
	}
}

//Attaches a GimpParasite to the GimpImage
static gboolean ll_parasite_attach()
{
 GimpParasite	*lg_l_parasite_attach, *lg_e_parasite_attach;
 GimpParasite	*undo_parasite_attach;
 GimpParasite	*tags_parasite_attach;
 gint		*data_lg_l_attach, * temp_l;
 gint		*data_lg_e_attach, * temp_e;
 gint		*data_undo_attach, * temp_undo;
 gint		*data_tags_attach, * temp_tags;
 gint		i,j;
 gint		undo_mem_count, temp_count;


	lg_l_parasite_attach = (GimpParasite *)malloc(sizeof(GimpParasite));
	lg_e_parasite_attach = (GimpParasite *)malloc(sizeof(GimpParasite));
	//undo_parasite_attach = (GimpParasite *)malloc(sizeof(GimpParasite));
	tags_parasite_attach = (GimpParasite *)malloc(sizeof(GimpParasite));

	data_lg_l_attach = (gint *)malloc(num_regions * layer_num * sizeof(gint));
	temp_l = (gint *)malloc(num_regions * layer_num * sizeof(gint));

	data_lg_e_attach = (gint *)malloc(num_regions * num_regions * sizeof(gint));
	temp_e = (gint *)malloc(num_regions * num_regions * sizeof(gint));

	undo_mem_count = 1; //1 for undo_index
	temp_count = 0;
	for(i = 0; i <= undo_index; i++)
	{
		//2 for for call_type and call_count
		//undo_array[i].call_count * 3 for flips
		temp_count = temp_count + 2 + (undo_array[i].call_count + 1) * 3;
	}
	undo_mem_count = undo_mem_count + temp_count;

	//1 for undo_mem_count
	undo_mem_count = undo_mem_count + 1;

	data_undo_attach = (gint *)malloc(undo_mem_count * sizeof(gint));
	temp_undo = (gint *)malloc(undo_mem_count * sizeof(gint));

	data_tags_attach = (gint *)malloc(image_height * image_width * sizeof(gint));
	temp_tags = (gint *)malloc(image_height * image_width * sizeof(gint));


	temp_l = data_lg_l_attach;

	for(i = 0; i < num_regions; i++)
	{
		for(j = 0; j < layer_num; j++)
		{        
			*data_lg_l_attach = graph->lists[i][j];
			data_lg_l_attach++;
		}
	}


	lg_l_parasite_attach = gimp_parasite_new ("LIST_GRAPH_LISTS",TRUE,num_regions * layer_num * sizeof(gint), temp_l);

	temp_e = data_lg_e_attach;

	for(i = 0; i < num_regions; i++)
	{
		for(j = 0; j < num_regions; j++)
		{        
			*data_lg_e_attach = graph->edges[i][j];
			data_lg_e_attach++;
		}
	}


	lg_e_parasite_attach = gimp_parasite_new ("LIST_GRAPH_EDGES",TRUE,num_regions * num_regions * sizeof(gint), temp_e);


	temp_undo = data_undo_attach;

	*data_undo_attach = undo_mem_count;
	data_undo_attach++;

	*data_undo_attach = undo_index;
	data_undo_attach++;

	for(i = 0; i <= undo_index; i++)
	{
		*data_undo_attach = undo_array[i].call_type;
		data_undo_attach++;
		*data_undo_attach = undo_array[i].call_count;
		data_undo_attach++;
		for(j = 0; j <= undo_array[i].call_count; j++)
		{
			*data_undo_attach = undo_array[i].flips[j][0];
			data_undo_attach++;
			*data_undo_attach = undo_array[i].flips[j][1];
			data_undo_attach++;
			*data_undo_attach = undo_array[i].flips[j][2];
			data_undo_attach++;
		}
	}

	undo_parasite_attach = gimp_parasite_new ("UNDO_ARRAY",TRUE,undo_mem_count * sizeof(gint), temp_undo);

	temp_tags = data_tags_attach;

	for(i = 0; i < image_height; i++)
	{
		for(j = 0; j < image_width; j++)
		{        
			*data_tags_attach = tags[i * image_width + j];
			data_tags_attach++;
		}
	}


	tags_parasite_attach = gimp_parasite_new ("TAGS",TRUE,image_height * image_width * sizeof(gint), temp_tags);


	gimp_image_parasite_attach (image_id,lg_l_parasite_attach);
	gimp_image_parasite_attach (image_id,lg_e_parasite_attach);
	gimp_image_parasite_attach (image_id,undo_parasite_attach);
	gimp_image_parasite_attach (image_id,tags_parasite_attach);

}

//Checks whether there is a preexisting parasite attached by a previous session of Local Layering 
static gboolean ll_parasite_exists()
{
 GimpParasite * lg_l_parasite, * lg_e_parasite;
 GimpParasite * undo_parasite;
 GimpParasite * tags_parasite;


	lg_l_parasite = (GimpParasite *)malloc(sizeof(GimpParasite));
	lg_e_parasite = (GimpParasite *)malloc(sizeof(GimpParasite));
	undo_parasite = (GimpParasite *)malloc(sizeof(GimpParasite));
	tags_parasite = (GimpParasite *)malloc(sizeof(GimpParasite));	


	lg_l_parasite = gimp_image_parasite_find (image_id,"LIST_GRAPH_LISTS");
	lg_e_parasite = gimp_image_parasite_find (image_id,"LIST_GRAPH_EDGES");
	undo_parasite = gimp_image_parasite_find (image_id,"UNDO_ARRAY");
	tags_parasite = gimp_image_parasite_find (image_id,"TAGS");

	if(lg_l_parasite == NULL)
	{
		return FALSE;
	}

	if(lg_e_parasite == NULL)
	{
		return FALSE;
	}

	if(undo_parasite == NULL)
	{
		return FALSE;
	}

	if(tags_parasite == NULL)
	{
		return FALSE;
	}

	return TRUE;
}


//Recovers data attached from a preexisting parasite attached by a previous session of Local Layering
static void ll_parasite_recover()
{
 GimpParasite * lg_l_parasite, * lg_e_parasite;
 GimpParasite * undo_parasite;
 GimpParasite * tags_parasite;

 gint * data_lg_l_attach;
 gint * data_lg_e_attach;

 gint * data_undo_attach;
 gint * data_tags_attach;

 gint i,j;
 gint undo_mem_count;


	lg_l_parasite = (GimpParasite *)malloc(sizeof(GimpParasite));
	lg_e_parasite = (GimpParasite *)malloc(sizeof(GimpParasite));
	undo_parasite = (GimpParasite *)malloc(sizeof(GimpParasite));
	tags_parasite = (GimpParasite *)malloc(sizeof(GimpParasite));	


	data_lg_l_attach = (gint *)malloc(num_regions * layer_num * sizeof(gint));
	data_lg_e_attach = (gint *)malloc(num_regions * num_regions * sizeof(gint));
	data_undo_attach = (gint *)malloc(1 * sizeof(gint));
	data_tags_attach = (gint *)malloc(image_height * image_width * sizeof(gint));


	lg_l_parasite = gimp_image_parasite_find (image_id,"LIST_GRAPH_LISTS");
	lg_e_parasite = gimp_image_parasite_find (image_id,"LIST_GRAPH_EDGES");
	undo_parasite = gimp_image_parasite_find (image_id,"UNDO_ARRAY");
	tags_parasite = gimp_image_parasite_find (image_id,"TAGS");

	//g_printf("\n\nPARASITE LISTS : %s\n",lg_l_parasite->name);

	data_lg_l_attach = lg_l_parasite->data;

	for(i = 0; i < num_regions; i++)
	{
		for(j = 0; j < layer_num; j++)
		{        
			graph->lists[i][j] = *data_lg_l_attach;
			data_lg_l_attach++;
		}
	}


	//g_printf("\n\nPARASITE EDGES : %s\n",lg_e_parasite->name);

	data_lg_e_attach = lg_e_parasite->data;

	for(i = 0; i < num_regions; i++)
	{
		for(j = 0; j < num_regions; j++)
		{        
			graph->edges[i][j] = *data_lg_e_attach;
			data_lg_e_attach++;
		}
	}


	//g_printf("\n\nPARASITE UNDO_ARRAY : %s\n",undo_parasite->name);

	data_undo_attach = undo_parasite->data;
	undo_mem_count = *data_undo_attach;;

	data_undo_attach = (gint *)malloc(undo_mem_count * sizeof(gint));
	data_undo_attach = undo_parasite->data;

	undo_mem_count = *data_undo_attach;;
	data_undo_attach++;

	undo_index = *data_undo_attach;
	data_undo_attach++;

	for(i = 0; i <= undo_index; i++)
	{
		undo_array[i].call_type = *data_undo_attach;
		data_undo_attach++;

		undo_array[i].call_count = *data_undo_attach;
		data_undo_attach++;

		for(j = 0; j <= undo_array[i].call_count; j++)
		{
			undo_array[i].flips[j][0] = *data_undo_attach;
			data_undo_attach++;

			undo_array[i].flips[j][1] = *data_undo_attach;
			data_undo_attach++;

			undo_array[i].flips[j][2] = *data_undo_attach;
			data_undo_attach++;
		}

	}
	
	//g_printf("\n\nPARASITE TAGS : %s\n",tags_parasite->name);

	data_tags_attach = tags_parasite->data;

	for(i = 0; i < image_height; i++)
	{
		for(j = 0; j < image_width; j++)
		{        
			old_tags[i * image_width +j] = *data_tags_attach;
			data_tags_attach++;
		}
	}


//return TRUE;
}

//Detaches the preexisting parasite attached by a previous session of Local Layering
//this is required to be done prior to reattachment of a freshly modified parasite 
static void ll_parasite_detach()
{
	gimp_image_parasite_detach (image_id,"LIST_GRAPH_LISTS");
	gimp_image_parasite_detach (image_id,"LIST_GRAPH_EDGES");
	gimp_image_parasite_detach (image_id,"UNDO_ARRAY");
	gimp_image_parasite_detach (image_id,"TAGS");
}


//Calculates and Selects a Region Boundary of the region the current cursor points to
static void rg_boundary_rect()
{
 gint	*rg_boungary, rg_tag_rb;
 gint	min_x, min_y, max_x, max_y;
 gint	i, j, k;
	
	rg_boundary = (gint *)malloc(image_height * image_width * sizeof(gint));

	k = 0;

	rg_tag_rb = tags[pos_y * image_width + pos_x];

	for(i = 0; i < image_height; i++)
	{
		for(j = 0; j < image_width; j++)
		{

			if(tags[i * image_width + j] == rg_tag_rb)
			{

				if((i == 0 || j == 0 || i == (image_height-1) || j == (image_width-1)) ||
				   (tags[(i-1) * image_width + j] != rg_tag_rb || 
				    tags[i * image_width + (j-1)] != rg_tag_rb ||
				    tags[i * image_width + (j+1)] != rg_tag_rb ||
				    tags[(i+1) * image_width + j] != rg_tag_rb ))
				{
					rg_boundary[k] = i;
					rg_boundary[k+1] = j;
					k = k + 2;
				}
			}
		}

	}

	min_x = image_width;

	min_y = image_height;

	max_x = 0;

	max_y = 0;

	for(i = 0; i < k; i = i + 2)
	{

		if(rg_boundary[i] < min_y)
		{
			min_y = rg_boundary[i];
		}

		if(rg_boundary[i+1] < min_x)
		{
			min_x = rg_boundary[i+1];
		}

		if(rg_boundary[i] > max_y)
		{
			max_y = rg_boundary[i];
		}

		if(rg_boundary[i+1] > max_x)
		{
			max_x = rg_boundary[i+1];
		}
	}

	gimp_rect_select (image_id, min_x, min_y, max_x - min_x + 1, max_y - min_y + 1, GIMP_CHANNEL_OP_REPLACE, FALSE,0);

}

//Calculates and Selects a Region Boundary of the regions affected by a Flip up or Flip Down call
//as per the contents of reg_affected array
static void rg_boundary_rect_regions()
{
 gint	rg_tag_rb;
 gint	min_x, min_y, max_x, max_y;
 gint	i, j, k, l;


	if(rg_boundary_call != 0)
	{
		rg_boundary_rect();
	}
	else
	{
		for(l = 0; l < num_regions; l++)
		{
			if(reg_affected[l])
			{
		
				rg_boundary = (gint *)malloc(image_height * image_width * sizeof(gint));
	
				rg_tag_rb = l + 1;

				k = 0;
				for(i = 0; i < image_height; i++)
				{
					for(j = 0; j < image_width; j++)
					{
	
						if(tags[i * image_width + j] == rg_tag_rb)
						{

					if((i == 0 || j == 0 || i == (image_height-1) || j == (image_width-1)) ||
					   (tags[(i-1) * image_width + j] != rg_tag_rb || 
					    tags[i * image_width + (j-1)] != rg_tag_rb ||
					    tags[i * image_width + (j+1)] != rg_tag_rb ||
					    tags[(i+1) * image_width + j] != rg_tag_rb ))
							{
								rg_boundary[k] = i;
								rg_boundary[k+1] = j;
								k = k + 2;
							}
						}
					}

				}



				min_x = image_width;
				min_y = image_height;
				max_x = 0;
				max_y = 0;

				for(i = 0; i < k; i = i + 2)
				{
					if(rg_boundary[i] < min_y)
					{
						min_y = rg_boundary[i];
					}

					if(rg_boundary[i+1] < min_x)
					{
						min_x = rg_boundary[i+1];
					}
	
					if(rg_boundary[i] > max_y)
					{
						max_y = rg_boundary[i];
					}

					if(rg_boundary[i+1] > max_x)
					{
						max_x = rg_boundary[i+1];
					}
				}

				gimp_rect_select (image_id, min_x, min_y, max_x - min_x + 1, max_y - min_y + 1, GIMP_CHANNEL_OP_ADD, FALSE,0);

			}
		}

		rg_boundary_call++;

	}

}

// Destroys the masks ie. removes the masks from the layers
static void destroy_masks()
{
	gint i;

	for(i = 0; i < layer_num; i++)
	{
		if(gimp_layer_get_mask ((layer[i]).id) != -1)
		{
			gimp_layer_remove_mask((layer[i]).id, GIMP_MASK_DISCARD);
			//gimp_layer_remove_mask(mask[i].layer_id, GIMP_MASK_DISCARD);
		}
	}
} 


//Prints a warning if the number of recursive calls for a particular flip call eceeds UNDO_FLIP_COUNT
//This would exceed the values the UNDO array would be able to hold
//Not an issue with the Local LAyering plug-in
//Simply increase the UNDO_FLIP_COUNT macros
static void print_warning()
{
	g_printf("\n\nEVIL PLUG-IN :|\n");
	g_printf("\nMAX Recursive calls exceeded for undoing of flips");
	g_printf("\nAction unable to complete");
	g_printf("\n\nSOLUTION :):)\n");
	g_printf("\nKindly increment the macros UNDO_FLIP_COUNT substantially ie.");
	g_printf("\n#define UNDO_FLIP_COUNT %d",32767);
	g_printf("\nand recompile the code before rerunning the Plug-in");
	g_printf("\n\nLocal Layering Shutting Down :(\n");

	//g_printf("\n\nAttempting MAXIMUM Undoing :)");
	//undo_array[undo_index].call_count = UNDO_FLIP_COUNT - 1;
}

//Prints the 2D layer_code array
static void print_layer_code()
{
 gint	i, j;
	for(i = 0; i < image_height; i++)
	 {
		g_printf("\n");
		for(j = 0; j < image_width; j++)
		 {        
			g_printf("%d",layer_code[i][j]);
		 }
	 }

}

//Prints the tags array
static void print_tags()
{
 gint	i, j;
	for(i = 0; i < image_height; i++)
	{
		g_printf("\n");
		for(j = 0; j < image_width; j++)
		{        
			g_printf("%d",tags[i * image_width + j]);
		}
	}
}

//prints the contents of ListGraph Lists
static void print_graph_lists()
{
 gint	i, j;
	for(i = 0; i < num_regions; i++)
		for(j = 0 ; j < layer_num ; j++)
			if(graph->lists[i][j] != 0)
				g_printf("\nRegion %2d --> Layer %2d - %2d ",i+1,j+1,graph->lists[i][j]);
}

//prints the contents of ListGraph Edges
static void print_graph_edges()
{
 gint	i, j;
	for(i = 0; i < num_regions; i++)
		for(j = 0 ; j < num_regions; j++)
			if(graph->edges[i][j] == 1)
				g_printf("\nRegion %2d --> Region %2d  (Edge)",i+1,j+1);
}

