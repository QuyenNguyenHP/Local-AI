"""Request-scoped progress logs, including work dispatched with asyncio.to_thread."""
from contextvars import ContextVar
from functools import wraps
import logging
from time import perf_counter

request_id = ContextVar('request_id', default='-')
logger = logging.getLogger('uvicorn.error')


def log(message, *args):
    logger.info('[%s] ' + message, request_id.get(), *args)


def stage(name):
    def decorate(function):
        @wraps(function)
        async def wrapped(*args, **kwargs):
            start = perf_counter()
            log('%s | started', name)
            try:
                result = await function(*args, **kwargs)
            except Exception:
                logger.exception('[%s] %s | failed after %.2fs', request_id.get(), name, perf_counter() - start)
                raise
            log('%s | completed in %.2fs', name, perf_counter() - start)
            return result
        return wrapped
    return decorate
