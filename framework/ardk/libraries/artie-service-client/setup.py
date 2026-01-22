from setuptools import setup
setup(
    name='artie-service-client',
    version="0.0.1",
    python_requires=">=3.10",
    license="MIT",
    packages=[
        "artie_service_client",
        "artie_service_client.interfaces",
    ],
    package_dir={
        "artie_service_client": "src/artie_service_client",
        "artie_service_client.interfaces": "src/artie_service_client/interfaces",
    },
    install_requires=[
        "artie-util",
        "rpyc==6.0.1",
    ]
)
