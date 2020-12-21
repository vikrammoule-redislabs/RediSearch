#include "profile.h"

void printReadIt(RedisModuleCtx *ctx, IndexIterator *root, size_t counter, double cpuTime) {
  IndexReader *ir = root->ctx;
  int verbose = PROFILE_VERBOSE;

  RedisModule_ReplyWithArray(ctx, 3 + verbose);

  if (ir->idx->flags & Index_DocIdsOnly) {
    RedisModule_ReplyWithSimpleString(ctx, "Tag reader");
    RedisModule_ReplyWithSimpleString(ctx, ir->record->term.term->str);
  } else if (ir->idx->flags & Index_StoreNumeric) {
    NumericFilter *flt = ir->decoderCtx.ptr;
    if (!flt || flt->geoFilter == NULL) {
      RedisModule_ReplyWithSimpleString(ctx, "Numeric reader");
      RedisModule_ReplyWithDouble(ctx, ir->record->num.value);
    } else {
      RedisModule_ReplyWithSimpleString(ctx, "Geo reader");
      RedisModule_ReplyWithDouble(ctx, ir->record->num.value);
    }
  } else {
    RedisModule_ReplyWithSimpleString(ctx, "Term reader");
    RedisModule_ReplyWithSimpleString(ctx, ir->record->term.term->str);
  }
  RedisModule_ReplyWithLongLong(ctx, counter);
  if (verbose) {
      RedisModule_ReplyWithLongDouble(ctx, cpuTime);
  }
}

static double _recursiveProfilePrint(RedisModuleCtx *ctx, ResultProcessor *rp, size_t *arrlen) {
  if (rp == NULL) {
    return 0;
  }
  double upstreamTime = _recursiveProfilePrint(ctx, rp->upstream, arrlen);

  // Array is filled backward in pair of [common, profile] result processors
  if (strcmp(rp->name, "Profile") != 0) {
    RedisModule_ReplyWithArray(ctx, 3);
    RedisModule_ReplyWithSimpleString(ctx, rp->name);
    //++*arrlen;
    return upstreamTime;
  }
  double totalTime = (double)RPProfile_GetClock(rp) / CLOCKS_PER_MILLISEC;
  RedisModule_ReplyWithLongLong(ctx, RPProfile_GetCount(rp));
  RedisModule_ReplyWithDouble(ctx, totalTime - upstreamTime);
  ++(*arrlen);
  return totalTime;
}

static double printProfileRP(RedisModuleCtx *ctx, ResultProcessor *rp, size_t *arrlen) {
  return _recursiveProfilePrint(ctx, rp, arrlen);
}

int Profile_Print(RedisModuleCtx *ctx, AREQ *req, size_t *nelem){
  // Print total time
  RedisModule_ReplyWithArray(ctx, 2);
  RedisModule_ReplyWithSimpleString(ctx, "Total time");
  RedisModule_ReplyWithDouble(ctx, (double)(clock() - req->initTime) / CLOCKS_PER_MILLISEC);
  (*nelem)++;

  // Print query parsing and creation time
  RedisModule_ReplyWithArray(ctx, 2);
  RedisModule_ReplyWithSimpleString(ctx, "Parsing and iterator creation time");
  RedisModule_ReplyWithDouble(ctx, (double)req->parseTime / CLOCKS_PER_MILLISEC);
  (*nelem)++;

  // print into array with a recursive function over result processors
  ResultProcessor *rp = req->qiter.endProc;
  IndexIterator *root = QITR_GetRootFilter(&req->qiter);

  // Print profile of iterators
  RedisModule_ReplyWithArray(ctx, 2);
  RedisModule_ReplyWithSimpleString(ctx, "Iterators profile");
  printIteratorProfile(ctx, root, 0 ,0);
  (*nelem)++;

  // Print profile of result processors
  RedisModule_ReplyWithArray(ctx, REDISMODULE_POSTPONED_ARRAY_LEN);
  RedisModule_ReplyWithSimpleString(ctx, "Result processors profile");
  size_t alen = 1;
  printProfileRP(ctx, rp, &alen);
  RedisModule_ReplySetArrayLength(ctx, alen);
  (*nelem)++;

  return REDISMODULE_OK;
}