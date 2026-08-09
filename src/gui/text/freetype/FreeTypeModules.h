/*
 * FreeType modules compiled by FreeTypeSingleObject.c.
 *
 * Keep this list aligned with that compilation unit. It intentionally omits
 * drivers and renderers that AeroGUI's built-in text runtime does not use.
 */

FT_USE_MODULE(FT_Module_Class, autofit_module_class)
FT_USE_MODULE(FT_Driver_ClassRec, tt_driver_class)
FT_USE_MODULE(FT_Driver_ClassRec, cff_driver_class)
FT_USE_MODULE(FT_Module_Class, psaux_module_class)
FT_USE_MODULE(FT_Module_Class, psnames_module_class)
FT_USE_MODULE(FT_Module_Class, pshinter_module_class)
FT_USE_MODULE(FT_Module_Class, sfnt_module_class)
FT_USE_MODULE(FT_Renderer_Class, ft_smooth_renderer_class)
FT_USE_MODULE(FT_Renderer_Class, ft_raster1_renderer_class)
