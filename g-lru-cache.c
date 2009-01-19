/* g-lru-cache.c
 *
 * Copyright (C) 2009 - Christian Hergert
 *
 * This is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * This is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "g-lru-cache.h"

#define LRU_CACHE_PRIVATE(object)          \
    (G_TYPE_INSTANCE_GET_PRIVATE((object), \
    G_TYPE_LRU_CACHE,                      \
    GLruCachePrivate))

struct _GLruCachePrivate
{
	GStaticRWLock   rw_lock;
	guint           max_size;
	
	GHashTable     *hash_table;
	GEqualFunc      key_equal_func;
	GList          *newest;
	GList          *oldest;
	
	GLookupFunc     retrieve_func;
	
	gpointer        user_data;
	GDestroyNotify  user_destroy_func;
};

G_DEFINE_TYPE (GLruCache, g_lru_cache, G_TYPE_OBJECT);

static void
g_lru_cache_finalize (GObject *object)
{
	GLruCachePrivate *priv = LRU_CACHE_PRIVATE (object);
	
	if (priv->user_data && priv->user_destroy_func)
		priv->user_destroy_func (priv->user_data);
	
	priv->user_data = NULL;
	priv->user_destroy_func = NULL;
	
	g_hash_table_destroy (priv->hash_table);
	priv->hash_table = NULL;
	
	g_list_free (priv->newest);
	priv->newest = NULL;
	priv->oldest = NULL;
	
	G_OBJECT_CLASS (g_lru_cache_parent_class)->finalize (object);
}

static void
g_lru_cache_class_init (GLruCacheClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	
	object_class->finalize = g_lru_cache_finalize;

	g_type_class_add_private (object_class, sizeof (GLruCachePrivate));
}

static void
g_lru_cache_init (GLruCache *self)
{
	self->priv = LRU_CACHE_PRIVATE (self);
	
	self->priv->max_size = 1024;
	g_static_rw_lock_init (&self->priv->rw_lock);
}

static void
g_lru_cache_evict_n_oldest_locked (GLruCache *self, gint n)
{
	GList *victim;
	gint   i;
	
	for (i = 0; i < n; i++)
	{
		victim = self->priv->oldest;
		
		if (victim == NULL)
			return;
		
		if (victim->prev)
			victim->prev->next = NULL;
		
		self->priv->oldest = victim->prev;
		g_hash_table_remove (self->priv->hash_table, victim->data);
		
		if (self->priv->newest == victim)
			self->priv->newest = NULL;
		
		g_list_free1 (victim); /* victim->data is owned by hashtable */
	}
	
#if DEBUG
	g_assert (g_hash_table_size (self->priv->hash_table) == g_list_length (self->priv->newest));
#endif
}

GLruCache*
g_lru_cache_new (GHashFunc      hash_func,
                 GEqualFunc     key_equal_func,
                 GLookupFunc    retrieve_func,
                 GDestroyNotify key_destroy_func,
                 GDestroyNotify value_destroy_func,
                 gpointer       user_data,
                 GDestroyNotify user_destroy_func)
{
	GLruCache *self = g_object_new (G_TYPE_LRU_CACHE, NULL);
	
	self->priv->hash_table = g_hash_table_new_full (hash_func,
	                                                key_equal_func,
	                                                key_destroy_func,
	                                                value_destroy_func);
	
	self->priv->key_equal_func = key_equal_func;
	self->priv->retrieve_func = retrieve_func;
	self->priv->user_data = user_data;
	self->priv->user_destroy_func = user_destroy_func;
	
	return self;
}

void
g_lru_cache_set_max_size (GLruCache *self, guint max_size)
{
	g_return_if_fail (G_IS_LRU_CACHE (self));
	
	guint old_max_size = self->priv->max_size;
	
	g_static_rw_lock_writer_lock (&(self->priv->rw_lock));
	
	self->priv->max_size = max_size;
	
	if (old_max_size > max_size)
		g_lru_cache_evict_n_oldest_locked (self, old_max_size - max_size);
	
	g_static_rw_lock_writer_unlock (&(self->priv->rw_lock));
}

guint
g_lru_cache_get_max_size (GLruCache *self)
{
	g_return_val_if_fail (G_IS_LRU_CACHE (self), -1);
	return self->priv->max_size;
}

guint
g_lru_cache_get_size (GLruCache *self)
{
	g_return_val_if_fail (G_IS_LRU_CACHE (self), -1);
	return g_hash_table_size (self->priv->hash_table);
}

gpointer
g_lru_cache_get (GLruCache *self, gpointer key)
{
	g_return_val_if_fail (G_IS_LRU_CACHE (self), NULL);
	
	gpointer value;
	
	g_static_rw_lock_reader_lock (&(self->priv->rw_lock));
	
	value = g_hash_table_lookup (self->priv->hash_table, key);
	
#if DEBUG
	if (value)
		g_debug ("Cache Hit!");
	else
		g_debug ("Cache miss");
#endif
	
	g_static_rw_lock_reader_unlock (&(self->priv->rw_lock));
	
	if (!value)
	{
		g_static_rw_lock_writer_lock (&(self->priv->rw_lock));
		
		if (!g_hash_table_lookup (self->priv->hash_table, key))
		{
			if (g_hash_table_size (self->priv->hash_table) >= self->priv->max_size)
#if DEBUG
			{
				g_debug ("We are at capacity, must evict oldest");
#endif
				g_lru_cache_evict_n_oldest_locked (self, 1);
#if DEBUG
			}
			
			g_debug ("Retrieving value from external resource");
#endif

			value = self->priv->retrieve_func (key, self->priv->user_data);
			
			g_hash_table_insert (self->priv->hash_table, key, value);
			
			self->priv->newest = g_list_prepend (self->priv->newest, key);
			
			if (self->priv->oldest == NULL)
				self->priv->oldest = self->priv->newest;
		}
#if DEBUG
		else g_debug ("Lost storage race with another thread");
#endif
		
		g_static_rw_lock_writer_unlock (&(self->priv->rw_lock));
	}
	
	return value;
}

void
g_lru_cache_evict (GLruCache *self, gpointer key)
{
	g_return_if_fail (G_IS_LRU_CACHE (self));
	
	GEqualFunc  equal = self->priv->key_equal_func;
	GList      *list  = NULL;
	
	g_static_rw_lock_writer_lock (&(self->priv->rw_lock));
	
	if (equal (key, self->priv->oldest))
	{
		g_lru_cache_evict_n_oldest_locked (self, 1);
	}
	else
	{
		g_hash_table_remove (self->priv->hash_table, key);
		
		for (list = self->priv->newest; list; list = list->next)
		{
			if (equal (key, list->data))
			{
				self->priv->newest = g_list_remove_link (self->priv->newest, list);
				g_list_free (list);
				break;
			}
		}
	}
	
	g_static_rw_lock_writer_unlock (&(self->priv->rw_lock));
}

void
g_lru_cache_clear (GLruCache *self)
{
	g_return_if_fail (G_IS_LRU_CACHE (self));
	
	g_static_rw_lock_writer_lock (&(self->priv->rw_lock));
	
	g_hash_table_remove_all (self->priv->hash_table);
	g_list_free (self->priv->newest);
	
	self->priv->oldest = NULL;
	self->priv->newest = NULL;
	
	g_static_rw_lock_writer_unlock (&(self->priv->rw_lock));
}

