import setuptools

with open("README.md", "r") as fh:
    long_description = fh.read()

setuptools.setup(
    name="sihd",
    version="0.0.1",
    author="Maxence Dufaud",
    author_email="maxence_dufaud@hotmail.fr",
    description="A Simple Input Handler and Displayer",
    long_description=long_description,
    long_description_content_type="text/markdown",
    licence="GPLv3+",
    url="https://github.com/mdufaud/sihd",
    packages=setuptools.find_packages(),
    classifiers=[
        "Programming Language :: Python :: 3",
        "Intended Audience :: Developpers",
        "Natural Language :: English",
        "Topic :: Software Development :: Libraries :: Python Modules",
        "License :: OSI Approved :: GNU General Public License v3 or later (GPLv3+)",
        "Operating System :: Mostly Unix",
    ],
    python_requires='>=3.5',
)
