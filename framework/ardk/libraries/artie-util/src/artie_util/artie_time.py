"""
Module for working with timestamps in Artie.

Anyone recording time should make use of the functions
in this module.
"""
import datetime

def str_to_datetime(s: str) -> datetime.datetime:
    """
    Convert a timestamp string in the standard Artie format to a datetime object.
    The format is ISO 8601 with microsecond precision and a 'Z' suffix to indicate UTC time.
    Example: "2024-01-01T12:00:00.123456Z" -> datetime.datetime(2024, 1, 1, 12, 0, 0, 123456)
    """
    return datetime.datetime.strptime(s, "%Y-%m-%dT%H:%M:%S.%fZ")

def datetime_to_str(dt: datetime.datetime) -> str:
    """
    Convert a datetime object to a timestamp string in the standard Artie format.
    The format is ISO 8601 with microsecond precision and a 'Z' suffix to indicate UTC time.
    Example: datetime.datetime(2024, 1, 1, 12, 0, 0, 123456) -> "2024-01-01T12:00:00.123456Z"
    """
    return dt.strftime("%Y-%m-%dT%H:%M:%S.%fZ")

def now_datetime() -> datetime.datetime:
    """
    Get the current time as a datetime object in UTC.

    This is a convenience function that just calls datetime.datetime.now(datetime.timezone.utc).
    """
    return datetime.datetime.now(datetime.timezone.utc)

def now_str() -> str:
    """
    Get the current time as a timestamp string in the standard Artie format.

    This is a convenience function that combines datetime.datetime.now(datetime.timezone.utc) and datetime_to_str().
    """
    return datetime_to_str(datetime.datetime.now(datetime.timezone.utc))

def str_to_datetime_local(s: str) -> datetime.datetime:
    """
    Convert a timestamp string in the standard Artie format to a datetime object in local time.
    The format is ISO 8601 with microsecond precision and a 'Z' suffix to indicate UTC time.
    Example: "2024-01-01T12:00:00.123456Z" -> datetime.datetime(2024, 1, 1, 7, 0, 0, 123456) (if local time is UTC-5)

    **Please Note**: Timestamps should always be recorded in UTC time in Artie.
    This function is provided for when you want to convert a timestamp to local time for display purposes,
    but it should not be used when recording timestamps to the database.
    """
    dt_utc = str_to_datetime(s)
    return dt_utc.astimezone(None)  # Convert to local time

def datetime_to_str_local(dt: datetime.datetime) -> str:
    """
    Convert a datetime object in local time to a timestamp string in the standard Artie format.
    The format is ISO 8601 with microsecond precision and a 'Z' suffix to indicate UTC time.
    Example: datetime.datetime(2024, 1, 1, 7, 0, 0, 123456) (if local time is UTC-5) -> "2024-01-01T12:00:00.123456Z"

    **Please Note**: Timestamps should always be recorded in UTC time in Artie.
    This function is provided for when you want to convert a local datetime to a UTC timestamp string for display purposes,
    but it should not be used when recording timestamps to the database.
    """
    dt_utc = dt.astimezone(datetime.timezone.utc)
    return datetime_to_str(dt_utc)

def now_datetime_local() -> datetime.datetime:
    """
    Get the current time as a datetime object in local time.

    This is a convenience function that just calls datetime.datetime.now().

    **Please Note**: Timestamps should always be recorded in UTC time in Artie.
    This function is provided for when you want to get the current local time for display purposes,
    but it should not be used when recording timestamps to the database.
    """
    return datetime.datetime.now()

def now_str_local() -> str:
    """
    Get the current time as a timestamp string in the standard Artie format, but in local time.

    This is a convenience function that combines datetime.datetime.now() and datetime_to_str_local().

    **Please Note**: Timestamps should always be recorded in UTC time in Artie.
    This function is provided for when you want to get the current local time as a UTC timestamp string for display purposes,
    but it should not be used when recording timestamps to the database.
    """
    return datetime_to_str_local(datetime.datetime.now())
