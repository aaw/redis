#include "redis.h"

void kaddCommand(redisClient *c) {
    robj *kset;
    int j;
    
    kset = lookupKeyWrite(c->db,c->argv[1]);
    if (kset == NULL) {
        kset = createKsetObject();
        dbAdd(c->db,c->argv[1],kset);
    } else {
        if (kset->type != REDIS_KSET) {
            addReply(c,shared.wrongtypeerr);
            return;
        }
    }
    
    for (j = 2; j < c->argc; j++) {
        c->argv[j] = tryObjectEncoding(c->argv[j]);
        hyperloglogAdd((hyperloglog*)kset->ptr, (unsigned char*)c->argv[j]->ptr, sdslen((sds)c->argv[j]->ptr)); 
    }
    signalModifiedKey(c->db,c->argv[1]);
    server.dirty++;
    addReply(c,shared.ok);
}

void kcardCommand(redisClient *c) {
    robj *o;
    if ((o = lookupKeyReadOrReply(c,c->argv[1],shared.czero)) == NULL || checkType(c,o,REDIS_KSET)) 
        return;

    addReplyLongLong(c,hyperloglogCard((hyperloglog*)o->ptr));
}

static uint64_t kunioncardHelper(redisClient *c, robj** sets, int setcount, hyperloglog* result) {
    int i, j;
    int8_t sketch_counter;
    hyperloglog* set;
    
    for (j = 0; j < HLL_M; j++) {
        result->contents[j] = 0;
    }
    
    for (i = 0; i < setcount; i++) {
        if (!sets[i]) continue;
        set = (hyperloglog*)sets[i]->ptr;
        for (j = 0; j < HLL_M; j++) {
            sketch_counter = set->contents[j];
            if (sketch_counter > result->contents[j])
                result->contents[j] = sketch_counter;
        }
    }
    
    return(hyperloglogCard(result));
}

void kunioncardCommand(redisClient *c) {
    int i;
    int setcount = c->argc-1;
    robj **setkeys = c->argv+1;
    robj **sets = zmalloc(sizeof(robj*)*setcount);
    robj *setobj;
    hyperloglog* result = hyperloglogNew();
    
    for (i = 0; i < setcount; i++) {
        setobj = lookupKeyRead(c->db,setkeys[i]);
        if (!setobj) { 
            /* Ignore empty sets, since for any set S, S unioned 
               with the empty set is just S */
            sets[i] = NULL;
            continue; 
        }
        if (checkType(c,setobj,REDIS_KSET)) {
            zfree(sets);
            zfree(result);
            return;
        }
        sets[i] = setobj;
    }
    
    addReplyLongLong(c,kunioncardHelper(c, sets, setcount, result));
    zfree(sets);
    zfree(result);
}


void kintercardCommand(redisClient *c) {
    int i, j;
    int setcount = c->argc-1;
    robj **setkeys = c->argv+1;
    robj **sets = zmalloc(sizeof(robj*)*setcount);
    robj **tempsets = zmalloc(sizeof(robj*)*setcount);
    robj *setobj;
    hyperloglog* result = hyperloglogNew();
    int64_t sum = 0;
    
    for (i = 0; i < setcount; i++) {
        setobj = lookupKeyRead(c->db,setkeys[i]);
        if (!setobj || checkType(c,setobj,REDIS_KSET)) { 
            /* Intersecting anything with an empty set yields the empty set, 
               so return 0 here on empty set. */
            addReplyLongLong(c,0);
            zfree(sets);
            zfree(tempsets);
            zfree(result);
            return;
        }
        sets[i] = setobj;
    }
    
    /* Compute intersection size. We can't compute it directly from
       the sketches of the sets as with union, but we can express the
       size of the intersection as an alternating sum of sums of sizes
       of k-set unions, thanks to the principle of inclusion-exclusion.
       The disadvantage here is that, with intersections of large
       numbers of sets, the error bound is unknown (but reasonable in
       practice) and the complexity of computing the size of the 
       intersection is exponential in the number of sets involved. */
    for (i = 1; i < (1 << setcount); i++) {
        for (j = 0; j < setcount; j++) {
            tempsets[j] = (i & (1 << j)) ? sets[j] : NULL;
        }
        sum += (__builtin_parity(i) ? 1 : -1) * kunioncardHelper(c, tempsets, setcount, result);
    }
    
    addReplyLongLong(c,sum);
    zfree(sets);
    zfree(tempsets);
    zfree(result);
}
