#include "mex.h"
#include "matrix.h"

#include <stdio.h>
#include <exception>
#include <list>
#include <sys/stat.h>
#include <sys/param.h>
#include <errno.h>
#include "transform.hh"
#include <Iex.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#if !defined(TRUE) 
#define TRUE 1
#endif
#if !defined(FALSE)
#define FALSE 0
#endif

#ifndef mwIndex
#define mwIndex int
#endif

extern void _main();

const int numInputArgs  = 3; 
const int numOutputArgs = 1;

double getfloat(const char *param, const char *name, ...)
{
	double result; 
	char *end;
    
	end=NULL;
	result=strtod(param, &end);
    
	if (end != NULL && *end != 0)
	{
		va_list ap;
		va_start(ap, name);
		mexPrintf("Unable to parse %s as a floating point number. Found as\n", param);
		vfprintf(stderr, name, ap);
		va_end(ap);
        mexErrMsgTxt("Throwing error\n");
		return 0;
	}
    
	return result;
}

ctl_parameter_t get_ctl_parameter(const char ***_argv, int *_argc, int start_argc, const char *type, int count)
{
	ctl_parameter_t new_ctl_param;
	const char **argv = *_argv;
	int argc = *_argc;
    
	memset(&new_ctl_param, 0, sizeof(new_ctl_param));
    
	argv++;
	argc--;
    
	new_ctl_param.name = argv[0];
    
	argv++;
	argc--;
    
	new_ctl_param.count = count;
	for (int i = 0; i < new_ctl_param.count; i++)
	{
		new_ctl_param.value[i] = getfloat(argv[0], "value %d of %s parameter %s (absolute parameter %d)", i + 1, type, new_ctl_param.name, start_argc - argc);
		argc--;
		argv++;
	}
    
	*_argv = argv - 1;
	*_argc = argc + 1;
    
	return new_ctl_param;
}

struct file_format_t
{
	const char *name;
	format_t output_fmt;
};

file_format_t allowed_formats[] =
{
	{ "exr",    format_t("exr",   0) },
    { "exr16",  format_t("exr",  16) },
    { "exr32",  format_t("exr",  32) },
    { "aces",   format_t("aces", 16) },
	{ "dpx",    format_t("dpx",   0) },
	{ "dpx8",   format_t("dpx",   8) },
	{ "dpx10",  format_t("dpx",  10) },
	{ "dpx12",  format_t("dpx",  12) },
	{ "dpx16",  format_t("dpx",  16) },
	{ "tif",    format_t("tif",   0) },
	{ "tiff",   format_t("tiff",  0) },
	{ "tiff32", format_t("tiff", 32) },
	{ "tiff16", format_t("tiff", 16) },
	{ "tiff8",  format_t("tiff",  8) },
	{ "tif32",  format_t("tif",  32) },
	{ "tif16",  format_t("tif",  16) },
	{ "tif8",   format_t("tif",   8) },
	{ NULL,     format_t()           }
};

const format_t &find_format(const char *fmt, const char *message = NULL)
{
	const file_format_t *current = allowed_formats;
    
	while (current->name != NULL)
	{
		if (!strcmp(current->name, fmt))
		{
			return current->output_fmt;
		}
		current++;
	}
	mexPrintf("Unrecognized format '%s'%s", fmt, message ? message : ".");
    mexErrMsgTxt("Throwing error\n");
    // Error should be thrown before reaching here.
	return current->output_fmt;
}

int verbosity = 1;

// Function declarations.
void usagePrompt(const char*);


// Function definitions.
// -----------------------------------------------------------------
void mexFunction (int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[])
{
    int argc = 0;
    const char **argv;
    mwIndex i;
    int k, ncell;
    int j = 0;
    
    // Count inputs and check for char type
    
    for( k=0; k<nrhs; k++ )
    {
        if( mxIsCell( prhs[k] ) )
        {
            argc += ncell = mxGetNumberOfElements( prhs[k] );
            for( i=0; i<ncell; i++ )
                if( !mxIsChar( mxGetCell( prhs[k], i ) ) )
                    mexErrMsgTxt("Input cell element is not char");
        }
        else
        {
            argc++;
            if( !mxIsChar( prhs[k] ) )
                mexErrMsgTxt("Input argument is not char");
        }
    }
    // Construct argv
    
    argv = (const char **) mxCalloc( argc, sizeof(char *) );
    
    for( k=0; k<nrhs; k++ )
    {
        if( mxIsCell( prhs[k] ) )
        {
            ncell = mxGetNumberOfElements( prhs[k] );
            for( i=0; i<ncell; i++ )
                argv[j++] = mxArrayToString( mxGetCell( prhs[k], i )
                                            );
        }
        else
        {
            argv[j++] = mxArrayToString( prhs[k] );
        }
    }
    
    /*
    for(j = 0; j < argc; j++) {
        mexPrintf("Input %d = <%s>\n", j, argv[j]);
    }
    */
    
    
	try
	{  
		// list of ctl filenames and associated parameters
		CTLOperations ctl_operations;
        
		CTLParameters global_ctl_parameters;
		ctl_operation_t new_ctl_operation;
        
		// list of input images on which to operate
		std::list<const char *> input_image_files;
		char output_path[MAXPATHLEN + 1];
        
        Compression compression = Compression::compressionNamed("PIZ");
		format_t desired_format;
		format_t actual_format;
		float input_scale = 0.0;
		float output_scale = 0.0;
		bool force_overwrite_output_file = FALSE;
		bool noalpha = FALSE;
        
		int start_argc = argc;
        
		new_ctl_operation.filename = NULL;
                
		while (argc > 0)
		{
			if (!strncmp(argv[0], "-help", 2))
			{
				if (argc > 1)
				{
					usagePrompt(argv[1]);
				}
				else
				{
					usagePrompt(NULL);
				}
				return;
			}
            
			else if (!strncmp(argv[0], "-input_scale", 2))
			{
				if (argc == 1)
				{
					mexPrintf(
							"The -input_scale option requires an "
							"additional option specifying a scale\nvalue for the "
							"input file. see '-help scale' for additional "
							"details.\n");
					return;
				}
				else
				{
					char *end = NULL;
					input_scale = strtof(argv[1], &end);
					if (end != NULL && *end != 0)
					{
						mexPrintf(
								"Unable to parse '%s' as a floating "
								"point number for the '-input_scale'\nargument\n",
								argv[1]);
						return;
					}
					argv++;
					argc--;
				}
			}
			else if (!strncmp(argv[0], "-output_scale", 2))
			{
				if (argc == 1)
				{
					mexPrintf(
							"The -output_scale option requires an "
							"additional option specifying a scale\nvalue for the "
							"output file. see '-help scale' for additional "
							"details.\n");
					return;
				}
				else
				{
					char *end = NULL;
					output_scale = strtof(argv[1], &end);
					if (end != NULL && *end != 0)
					{
						mexPrintf(
								"Unable to parse '%s' as a floating "
								"point number for the '-output_scale'\nargument\n",
								argv[1]);
						return;
					}
					argv++;
					argc--;
				}
			}
			else if (!strncmp(argv[0], "-ctl", 3))
			{
				if (argc == 1)
				{
					mexPrintf(
							"the -ctl option requires an additional "
							"option specifying a file containing a\nctl script.\n"
							"see '-help ctl' for more details.\n");
					return;
				}
                
				if (new_ctl_operation.filename != NULL)
				{
					ctl_operations.push_back(new_ctl_operation);
				}
				new_ctl_operation.local.clear();
				new_ctl_operation.filename = argv[1];
                
				argv++;
				argc--;
			}
			else if (!strncmp(argv[0], "-format", 5))
			{
				if (argc == 1)
				{
					mexPrintf(
							"the -format option requires an additional "
							"argument specifying the destination file\nformat. "
							"this may be one of the following: 'dpx10', 'dpx16', "
							"'aces', 'tiff8',\n'tiff16', or 'exr'.\nSee '-help "
							"format' for more details.\n");
					return;
				}
				desired_format = find_format(argv[1]," for parameter '-format'.\nSee '-help format' for more details.");
				argv++;
				argc--;
			}
            else if (!strncmp(argv[0], "-compression", 3))
            {
                if (argc == 1)
                {
                    mexPrintf(
                            "the -compression option requires an additional "
                            "argument specifying a compression scheme to be "
                            "used.\n See '-help compression' for more details.\n");
                    return;
                }
                char scheme[8];
                memset(scheme, '\0', 8);
                for(int i = 0; i < 8 && argv[1][i]; ++i) {
                    scheme[i] = toupper(argv[1][i]);
                }
                compression = Compression::compressionNamed(scheme);
                if (!strcmp(compression.name, Compression::no_compression.name)) {
                    mexPrintf("Unrecognized compression scheme '%s'. Turning off compression.\n", scheme);
                }
                argv++;
                argc--;
            }
			else if (!strcmp(argv[0], "-param1") || !strcmp(argv[0], "-p1"))
			{
				if (argc < 3)
				{
					mexPrintf(
							"the -param1 option requires two additional "
							"arguments specifying the\nparameter name and value."
							"\nSee '-help param' for more details.\n");
					return;
				}
				if (new_ctl_operation.filename == NULL)
				{
					THROW(Iex::ArgExc, "the -param1 argument must occur *after* a -ctl option.");
				}
				new_ctl_operation.local.push_back(get_ctl_parameter(&argv, &argc, start_argc, "local", 1));
			}
			else if (!strcmp(argv[0], "-param2") || !strcmp(argv[0], "-p2"))
			{
				if (argc < 4)
				{
					mexPrintf(
							"the -param2 option requires three additional "
							"arguments specifying the\nparameter name and value."
							"\nSee '-help param' for more details.\n");
					return;
                    
				}
				if (new_ctl_operation.filename == NULL)
				{
					THROW(Iex::ArgExc, "the -param2 argument must occur *after* a -ctl option.");
				}
				new_ctl_operation.local.push_back(get_ctl_parameter(&argv, &argc, start_argc, "local", 3));
			}
			else if (!strcmp(argv[0], "-param3") || !strcmp(argv[0], "-p3"))
			{
				if (argc < 5)
				{
					mexPrintf(
							"the -param3 option requires four additional "
							"arguments specifying the\nparameter name and value."
							"\nSee '-help param' for more details.\n");
					return;
				}
				if (new_ctl_operation.filename == NULL)
				{
					THROW(Iex::ArgExc, "the -param3 argument must occur *after* a -ctl option.");
				}
				new_ctl_operation.local.push_back(get_ctl_parameter(&argv, &argc, start_argc, "local", 3));
			}
			else if (!strcmp(argv[0], "-global_param1") || !strcmp(argv[0], "-gp1"))
			{
				if (argc < 3)
				{
					mexPrintf("the -global_param1 option requires two "
							"additional arguments specifying the\nparameter "
							"name and value.\nSee '-help param' for more "
							"details.\n");
					return;
				}
				global_ctl_parameters.push_back(get_ctl_parameter(&argv, &argc, start_argc, "global", 1));
			}
			else if (!strcmp(argv[0], "-global_param2") || !strcmp(argv[0], "-gp2"))
			{
				if (argc < 4)
				{
					mexPrintf("the -global_param2 option requires three "
							"additional arguments specifying the\nparameter "
							"name and value.\nSee '-help param' for more "
							"details.\n");
					return;
				}
				global_ctl_parameters.push_back(get_ctl_parameter(&argv, &argc, start_argc, "global", 2));
			}
			else if (!strcmp(argv[0], "-global_param3") || !strcmp(argv[0], "-gp3"))
			{
				if (argc < 5)
				{
					mexPrintf("the -global_param3 option requires four "
							"additional arguments specifying the\nparameter "
							"name and value.\nSee '-help param' for more "
							"details.\n");
					return;
                    
				}
				global_ctl_parameters.push_back(get_ctl_parameter(&argv, &argc, start_argc, "global", 2));
			}
			else if (!strncmp(argv[0], "-verbose", 2))
			{
				verbosity++;
			}
			else if (!strncmp(argv[0], "-quiet", 2))
			{
				verbosity--;
			}
			else if (!strncmp(argv[0], "-force", 5))
			{
				force_overwrite_output_file = TRUE;
			}
			else if (!strncmp(argv[0], "-noalpha", 2))
			{
				noalpha = TRUE;
			}
			else if (!strncmp(argv[0], "-", 1))
			{
				mexPrintf(
						"unrecognized option %s. see -help for a list "
						"of available options.\n", argv[0]);
				return;
			}
			else
			{
				input_image_files.push_back(argv[0]);
			}
            
            
			argv++;
			argc--;
            
            
            
		}  // end while

		if (new_ctl_operation.filename != NULL)
		{
			ctl_operations.push_back(new_ctl_operation);
		}
		if (input_image_files.size() < 2)
		{
			mexPrintf(
					"one or more source filenames and a destination "
					"file or directory must be\nprovided. if more than one "
					"source filenames is provided then the last argument\nmust "
					"be a directory. see -help for more details.\n");
			return;
		}
        
		char *output_slash = NULL;
		const char *outputFile = input_image_files.back();
		input_image_files.pop_back();
        
		struct stat file_status;
		if (stat(outputFile, &file_status) >= 0)
		{
			if (S_ISDIR(file_status.st_mode))
			{
				memset(output_path, 0, sizeof(output_path));
				strncpy(output_path, outputFile, MAXPATHLEN);
				outputFile = output_path;
				output_slash = output_path + strlen(output_path);
				if (*output_slash != '/')
				{
					*(output_slash++) = '/';
					*output_slash = 0;
				}
			}
			else if (S_ISREG(file_status.st_mode))
			{
				if (input_image_files.size() > 1)
				{
					mexPrintf(
							"When providing more than one source "
							"image the destination must be a\ndirectory.\n");
					return;
				}
				else
				{
					if (!force_overwrite_output_file)
					{
						mexPrintf(
								"The destination file %s already exists.\n"
								"Cravenly refusing to overwrite unless you supply "
								"the -force option.\n", outputFile);
						return;
					}
					else
					{
						// File exists, but we treat it as if it doesn't (see
						// if(output_slash==NULL) {...} down below...
						output_slash = NULL;
					}
				}
			}
			else
			{
				mexPrintf(
						"Specified destination is something other than "
						"a file or directory. That's\nprobably a bad idea.\n");
				return;
			}
		}
		else
		{
			if (errno != ENOENT)
			{
				mexPrintf("Unable to get information about %s (%s).\n", outputFile, strerror(errno));
				return;
			}
			if (input_image_files.size() != 1)
			{
				mexPrintf(
						"When specifying more than one source file "
						"you must specify the destination as\na directory "
						"that already exists.\nUnable to stat '%s' (%s)\n",
						outputFile, strerror(errno));
				return;
			}
		}
        
		if (output_slash == NULL)
		{
			// This is the case when our outputFile is a single file. We do a bunch
			// of sanity checking between the extension of the specified file
			// (if any) and the -format option (if any).
			char *dot = (char *) strrchr(outputFile, '.');
			if (dot == NULL && desired_format.ext == NULL)
			{
				mexPrintf(
						"You have not explicitly provided an output "
						"format, and the output file name\ndoes not not contain "
						"an extension. Please add an extension to the output "
						"file\nor use the -format option to specify the desired "
						"output format.\n");
				return;
			}
			else if (dot != NULL)
			{
				if (desired_format.ext == NULL)
				{
					actual_format = find_format(dot + 1,
							                    " specified implicitly (by "
									            "the extension) for\noutput file "
									            "format. Please fix this or use\n"
									            "the -format option to specify "
									            "the desired output format.\n");
				}
				else
				{
					// HACK aces format file type check
                    const char *ext = desired_format.ext;
                    static const char exrext[] = "exr";
                    if (!strcmp(ext, "aces"))
                        ext = exrext;
                        if (strcmp(ext, dot + 1) && !force_overwrite_output_file)
                        {
                            mexPrintf(
                                    "You have specified a destination file "
                                    "type with the -format option, but the\noutput "
                                    "file extension does not match the format "
                                    "specified by the -format option.\nCravenly "
                                    "refusing to do this unless you specify the "
                                    "-force option (which\nwill make the -format "
                                    "option take priority).\n");
                            return;
                        }
					actual_format = desired_format;
				}
			}
		}
        
		if (verbosity > 1)
		{
			mexPrintf("global ctl parameters:\n");
            
			CTLParameters temp_ctl_parameters;
			temp_ctl_parameters = global_ctl_parameters;
            
			while (temp_ctl_parameters.size() > 0)
			{
				ctl_parameter_t new_ctl_parameter = temp_ctl_parameters.front();
				temp_ctl_parameters.pop_front();
				mexPrintf("%17s:", new_ctl_parameter.name);
				for (int i = 0; i < new_ctl_parameter.count; i++)
				{
					mexPrintf(" %f", new_ctl_parameter.value[i]);
				}
				mexPrintf("\n");
			}
			mexPrintf("\n");
		}
        
		while (input_image_files.size() > 0)
		{
			const char *inputFile = input_image_files.front();
            
			if (output_slash != NULL)
			{
				const char *input_slash = strrchr(inputFile, '/');
				if (input_slash == NULL)
				{
					input_slash = (char *) inputFile;
				}
				else
				{
					input_slash++;
				}
				strcpy(output_slash, input_slash);
				char *dot = (char *) strrchr(outputFile, '.');
				if (dot != NULL)
				{
					dot++;
					if (desired_format.ext != NULL)
					{
						// HACK aces format file type check
                        const char *ext = desired_format.ext;
                        static const char exrext[] = "exr";
                        if (!strcmp(ext, "aces"))
                            ext = exrext;
                            strcpy(dot, ext);
                            actual_format = desired_format;
                            }
					else
					{
						actual_format = find_format(dot, " (determined from destination file extension).");
					}
				}
			}
            
			if (force_overwrite_output_file)
			{
				if (unlink(outputFile) < 0)
				{
					if (errno != ENOENT)
					{
						mexPrintf("Unable to remove existing file named "
								"'%s' (%s).\n", outputFile, strerror(errno));
						return;
					}
				}
			}
			if (access(outputFile, F_OK) >= 0)
			{
                mexPrintf("Cravenly refusing to overwrite the file '%s'.\n", outputFile);
				return;
			}
			actual_format.squish = noalpha;
			transform(inputFile, outputFile, input_scale, output_scale, &actual_format, &compression, ctl_operations, global_ctl_parameters);
			input_image_files.pop_front();
		}
        
        
	} catch (std::exception &e)
	{
		mexPrintf("exception thrown (oops...): %s\n", e.what());
	}
    
    /*
    if( argc )
    {
        for( j=argc-1; j>=0; j-- )
            mxFree( argv[j] );
        mxFree( argv );
    }
    */
}


void usagePrompt(const char *section) {
	if(section==NULL) {
		mexPrintf(""
"ctlrender - transforms an image using one or more CTL scripts, potentially\n"
"            converting the file format in the process\n"
"\nusage:\n"
"    ctlrender [<options> ...] <source file...> <destination>\n"
"\n"
"\n"
"options:\n"
"\n"
"    <source file...>      One or more source files may be specified in a\n"
"                          space separated list. Note to non-cygwin using\n"
"                          Windows users: wild card ('*') expansions are not\n"
"                          supported.\n"
"\n"
"    <destination>         In the case that only one source file is specified\n"
"                          this may be either a filename or a directory. If\n"
"                          a file is specified, then the output format of the\n"
"                          file is determined from the input file type and\n"
"                          the extension of the output file type.\n"
"                          If more than one source file is specified then\n"
"                          this must specify an existing directory. To\n"
"                          perform a file type conversion, the '-format'\n"
"                          option must be used.\n"
"                          See below for details on the '-format' option.\n"
"\n"
"    -input_scale <value>  Specifies a scaling value for the input.\n"
"                          Details on this are provided with '-help scale'.\n"
"\n"
"    -output_scale <value> Specifies a scaling value for the output file.\n"
"                          Details on this are provided with '-help scale'.\n"
"\n"
"    -format <output_fmt>  Specifies the output file format. If ony one\n"
"                          source file is specified then the extension of\n"
"                          destination file is used to determine the file\n"
"                          format. Details on this are provided with\n"
"                          '-help format'\n"
"\n"
"    -compression <type>   Specifies OpenEXR compression type. Value will\n"
"                          be ignored when not saving an exr file\n"
"                          '-help compression'\n"
"\n"
"    -ctl <filename>       Specifies the name of a CTL file to be applied\n"
"                          to the input images. More than one CTL file may\n"
"                          be provided (each must be delineated by a '-ctl'\n"
"                          option), and they are applied in-order.\n"
"\n"
"    -param1 ...           Specifies the value of a CTL script parameter.\n"
"    -param2 ...           Details on this and similar options are provided\n"
"    -param3 ...           with '-help param'\n"
"\n"
"    -verbose              Increases the level of output verbosity.\n"
"    -quiet                Decreases the level of output verbosity.\n"
"");
	} else if(!strncmp(section, "format", 1)) {
		mexPrintf(""
"format conversion:\n"
"\n"
"    ctlrender provides file format conversion either implicitly by the\n"
"    extension of the output file, or via the use of the '-format' option.\n"
"    Valid values for the '-format' option are:\n"
"\n"
"        dpx10   Produces a DPX file with a 10 bits per sample (32 bit \n"
"                packed) format\n"
"\n"
"        dpx16   Produces a DPX file with a 16 bits per sample format\n"
"\n"
"        dpx     Produces a DPX file with the same bit depth as the source\n"
"                image\n"
"\n"
"        tiff8   Produces a TIFF file in the 8 bits per sample format\n"
"\n"
"        tiff16  Produces a TIFF file in the 16 bits per sample format\n"
"\n"
"        tiff32  Produces a TIFF file in the 32 bits per sample format\n"
"\n"
"        tiff    Produces a TIFF file with the same bit depth as the source\n"
"                image\n"
"\n"
"        exr16   Produces an exr file in the half (16 bit float) per sample\n"
"                format\n"
"\n"
"        exr32   Produces an exr file in the float (32 bit float) per sample\n"
"                format\n"
"\n"
"        exr     Produces an exr file with the same bit depth as the source\n"
"\n"
"        aces    Produces an aces compliant exr file\n"
"\n"
"    When only one source file is specified with a destination file name,\n"
"    the extension is interpreted the same way as an argument to '-format',\n"
"    and will not be changed.\n"
"\n"
"    When the destination is a directory and the -format is provided, the\n"
"    file extension will be changed to the type specified in the -format\n"
"    option with the bit depth removed.\n"
"\n"
"    Note that no automatic depth scaling is performed, please see\n"
"    '-help scale' for more details on how scaling is performed.\n"
"");
    } else if(!strncmp(section, "compression", 2)) {
#if defined(HAVE_OPENEXR)
        mexPrintf(""
"exr compression:\n"
"\n"
"    ctlrender provides the option of a compression scheme when saving an \n"
"    OpenEXR image. If '-compression' option is not given, PIZ will be used.\n"
"    Valid values for the '-compression' option are:\n"
"\n"
"        NONE    Do not compress.\n"
"\n"
"        PIZ     (lossless) Ideal for photographic images.\n"
"                Default compression scheme.\n"
"\n"
"        ZIPS    (lossless) ZIP one scanline at a time.\n"
"\n"
"        ZIP     (lossless) Ideal for texture maps.\n"
"\n"
"        RLE     (lossless) Ideal for images with large flat areas.\n"
"\n"
"        PXR24   (lossy) Ideal for images with a large range of values but\n"
"                full 32-bit accuracy is not necessary (e.g. depth buffer).\n"
"                HALF and UINT channels are preserved exactly.\n"
"\n"
"        B44     (lossy) Possibly advantageous to real-time playback systems.\n"
"\n"
"        B44A    (lossy) Like B44 but smaller for images containing large\n"
"                uniform areas.\n"
"");
#else
        mexPrintf(""
"exr compression:\n"
"\n"
"    ctlrender provides the option of a compression scheme when saving an \n"
"    OpenEXR image. If '-compression' option is not given, PIZ will be used.\n"
"    Valid values for the '-compression' option are:\n"
"\n"
"        NONE    Do not compress.\n"
"\n"
"    OpenEXR support must be enabled for the '-compression' option to be\n"
"    meaningful. Please see build documentation for details.\n"
"");
#endif
	} else if(!strncmp(section, "ctl", 1)) {
		mexPrintf(""
"ctl file interpretation:\n"
"    ctlrender treats all ctl files as if they take their input as 'R', 'G',\n"
"    'B', and 'A' (optional) channels, and produce output as 'R', 'G', and\n"
"    'B', and 'A' (if required) channels. In the event of a single channel\n"
"    input file only the 'G' channel will be used.\n"
"");
//"    The *LAST* function in the file is the function that will be called to\n"
//"    provide the transform. This is to maintain compatability with scripts\n"
//"    developed for Autodesk's TOXIC product.\n"
	} else if(!strncmp(section, "scale", 1)) {
		mexPrintf(""
"input and output value scaling:\n"
"\n"
"    To deal with differences in input and output file bit depth, the ability\n"
"    to scale input and output values has been provided. While these options\n"
"    are primarily of use for integral file formats, they can be used with\n"
"    file formats that store data in floating point or psuedo-floating point\n"
"    formats. The default handling of the input and output scaling is variant\n"
"    on the format of the input (and output) file, but is intended to behave \n"
"    as one expects.\n"
"\n"
"    integral input files (integer tiff, integer dpx):\n"
"        If the '-input_scale' option is provided then the sample value from\n"
"        the file is *divided by* the specified scale.\n"
"        If the '-input_scale' option is not provided, then the input values\n"
"        are scaled to the range 0.0-1.0 (inclusive). For the purposes of\n"
"        this argument, DPX files are considered an integral file format,\n"
"        however ACES files are *not*. This is equivalent to specifying\n"
"        -input_scale <bits_per_sample_in_input_file>\n"
"\n"    
"    floating point input files (exr, floatint point TIFF, floating point\n"
"    dpx):\n"
"        If the '-input_scale' option is provided then the sample values\n"
"        are *multiplied by* the scale value.\n"
"        If the '-input_scale' option is not provided then the sample values\n"
"        from the file is used as-is (with a scale of 1.0).\n"
"\n"
"    integral output files (integer tiff, integer dpx):\n"
"        If the '-output_scale' option is provided then the sample value from\n"
"        the CTL transformation is *multiplied by* the scale factor.\n"
"        If the '-output_scale' option is not provided, then the values of\n"
"        0.0-1.0 from the CTL transformation are scaled to the bit depth of\n"
"        the output file. For the purposes of this argument, DPX files are\n"
"        considered an integral file format, however ACES files are *not*.\n"
"        This is equivalent to specifying\n"
"        -output_scale <bits_per_sample_in_output_file>\n"
"\n"
"    floating point output files (exr, floatint point TIFF, floating point\n"
"    dpx):\n"
"        If the '-output_scale' option is provided then the sample values\n"
"        are *divided by* the scale value.\n"
"        If the '-output_scale' option is not provided then the sample values\n"
"        from the file is used as-is (with a scale of 1.0).\n"
"\n"
"    In all cases the CTL output values (after output_scaling) are clipped\n"
"    to the maximum values supported by the output file format.\n"
"");
	} else if(!strncmp(section, "param", 1)) {
		mexPrintf(""
"ctl parameters:\n"
"\n"
"    In CTL scripts it is possible to define parameters that are not set\n"
"    until runtime. These parameters take one, two, or three floating point\n"
"    values. There are three options that allow you to specify the name\n"
"    of the parameter and the associated values. The options are as follows:\n"
"\n"
"        -param1 <name> <float1>\n"
"        -param2 <name> <float1> <float2>\n"
"        -param3 <name> <float1> <float2> <float3>\n"
"\n"
"        -global_param1 <name> <float1>\n"
"        -global_param2 <name> <float1> <float2>\n"
"        -global_param3 <name> <float1> <float2> <float3>\n"
"");
	} else {
		mexPrintf(""
"The '%s' section of the help does not exist. Try running ctlrender with\n"
"only the -help option.\n"
"", section);
	}
}
