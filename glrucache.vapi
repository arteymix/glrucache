[CCode (cheader_filename = "g-lru-cache.h", lower_case_cprefix = "g_lru_")]
namespace GLru {
	public delegate V LookupFunc<K, V> (K key);
	public class Cache<K, V> : GLib.Object {
		public uint max_size { get; set; }
		public uint size { get; }
		public bool fast_get { get; set; }
		public Cache (GLib.HashFunc<K> hash_func, GLib.EqualFunc<K> equal_func, owned GLru.LookupFunc<K, V> retreve_func);
		public unowned V get (K key);
		public void evict (K key);
	}
}
