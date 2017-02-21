#include <stdlib.h>
#include <glib.h>

#include "g-lru-cache.h"

static void bigtest (GLruCache *cache);

gpointer
lookup (gpointer key, gpointer user_data)
{
	gint result = 0;
	
	if (g_str_equal (key, "a"))
		result = 1;
	else if (g_str_equal (key, "b"))
		result = 2;
	else if (g_str_equal (key, "c"))
		result = 3;
	else
		return GINT_TO_POINTER (42);
		
	return GINT_TO_POINTER (result);
}

gint
main (gint argc, gchar *argv[])
{
	GLruCache *cache = g_lru_cache_new (G_TYPE_STRING,
                                      (GCopyFunc)g_strdup,
                                      g_free,
                                      G_TYPE_INT,
                                      NULL,
                                      NULL,
                                      (GHashFunc)g_str_hash,
                                      (GEqualFunc)g_str_equal,
	                                    G_LOOKUP_FUNC (lookup),
                                      NULL,
                                      NULL);
	
	g_assert (cache != NULL);
	
	gint a = GPOINTER_TO_INT (g_lru_cache_get (cache, "a"));
	gint b = GPOINTER_TO_INT (g_lru_cache_get (cache, "b"));
	gint c = GPOINTER_TO_INT (g_lru_cache_get (cache, "c"));
	
	g_print ("a = %d\n", a);
	g_print ("b = %d\n", b);
	g_print ("c = %d\n", c);
	
	g_assert (a == 1);
	g_assert (b == 2);
	g_assert (c == 3);
	
	g_assert (g_lru_cache_get_size (cache) == 3);
	
	bigtest (cache);
	
	return EXIT_SUCCESS;
}

static void
bigtest (GLruCache *cache)
{
	gint i;
	
	for (i = 0; i < 2000; i++)
		g_lru_cache_get (cache, g_strdup_printf ("%d", i)); // ignore key leak for now

	g_assert (g_lru_cache_get_size (cache) == 1024);
	g_print ("size = %d\n", g_lru_cache_get_size (cache));
	
	// should be cache hit
	g_print ("Expecting cache hit\n");
	g_lru_cache_get (cache, "1800");
	g_print ("Check log?\n");
	
	// should be miss
	g_print ("Expecting cache miss\n");
	g_lru_cache_get (cache, "975");
	g_print ("Check log?\n");
	
	g_lru_cache_clear (cache);
	g_assert (g_lru_cache_get_size (cache) == 0);
	g_print ("size = %d\n", g_lru_cache_get_size (cache));
}

