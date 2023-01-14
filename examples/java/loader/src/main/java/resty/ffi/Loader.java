package resty.ffi;

import java.io.*;
import java.io.ByteArrayInputStream;
import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.sql.Connection;
import java.sql.PreparedStatement;
import java.sql.ResultSet;
import java.sql.SQLException;
import java.util.Arrays;
import java.util.Collection;
import java.util.HashSet;
import java.util.LinkedList;
import java.util.Set;
import java.util.jar.JarFile;
import java.util.zip.ZipEntry;

/**
 * Represent a function that accept one parameter and return value
 * @param <A> The only parameter
 * @param <T> The return value
 */
interface F1<A, T> {
    /**
     * Evaluate or execute the function
     * @param obj The parameter
     * @return Result of execution
     */
	T e(A obj);
}

class IOUtil {
	/**
	 * Read the stream into byte array
	 * @param inputStream
	 * @return
	 * @throws java.io.IOException
	 */
    public static byte[] readData(InputStream inputStream) {
        try {
			return readDataNice(inputStream);
		} finally {
        	close(inputStream);
		}
    }

    public static byte[] readDataNice(InputStream inputStream) {
		ByteArrayOutputStream boTemp = null;
        byte[] buffer = null;
        try {
            int read;
			buffer = new byte[8192];
            boTemp = new ByteArrayOutputStream();
            while ((read=inputStream.read(buffer, 0, 8192)) > -1) {
                boTemp.write(buffer, 0, read);
            }
            return boTemp.toByteArray();
        } catch (IOException e) {
			throw new RuntimeException(e);
        }
	}

    /**
     * Close streams (in or out)
     * @param stream
     */
    public static void close(Closeable stream) {
        if (stream != null) {
            try {
                if (stream instanceof Flushable) {
                    ((Flushable)stream).flush();
                }
                stream.close();
            } catch (IOException e) {
                // When the stream is closed or interupted, can ignore this exception
            }
        }
    }
}

abstract class AggressiveClassLoader extends ClassLoader {
    Set<String> loadedClasses = new HashSet<>();
    Set<String> unavaiClasses = new HashSet<>();
    private ClassLoader parent = AggressiveClassLoader.class.getClassLoader();
    String prefix;

    public AggressiveClassLoader(String clsName) {
        prefix = clsName;
    }

    @Override
    public Class<?> loadClass(String name) throws ClassNotFoundException {
        if (!name.startsWith(prefix)) {
            return super.loadClass(name);
        }

        if (loadedClasses.contains(name) || unavaiClasses.contains(name)) {
            return super.loadClass(name); // Use default CL cache
        }

        byte[] newClassData = loadNewClass(name);
        if (newClassData != null) {
            loadedClasses.add(name);
            return loadClass(newClassData, name);
        } else {
            unavaiClasses.add(name);
            return parent.loadClass(name);
        }
    }

    public AggressiveClassLoader setParent(ClassLoader parent) {
        this.parent = parent;
        return this;
    }

    public Class<?> load(String name) {
        try {
            return loadClass(name);
        } catch (ClassNotFoundException e) {
            throw new RuntimeException(e);
        }
    }

    protected abstract byte[] loadNewClass(String name);

    public Class<?> loadClass(byte[] classData, String name) {
        Class<?> clazz = defineClass(name, classData, 0, classData.length);
        if (clazz != null) {
            if (clazz.getPackage() == null) {
                definePackage(name.replaceAll("\\.\\w+$", ""), null, null, null, null, null, null, null);
            }
            resolveClass(clazz);
        }
        return clazz;
    }

    public static String toFilePath(String name) {
        return name.replaceAll("\\.", "/") + ".class";
    }
}

class DynamicClassLoader extends AggressiveClassLoader {
    LinkedList<F1<String, byte[]>> loaders = new LinkedList<>();

    public DynamicClassLoader(String[] paths, String clsName) {
        super(clsName);
        for (String path : paths) {
            File file = new File(path);

            F1<String, byte[]> loader = loader(file);
            if (loader == null) {
                throw new RuntimeException("Path not exists " + path);
            }
            loaders.add(loader);
        }
    }

    public static F1<String, byte[]> loader(File file) {
        if (!file.exists()) {
            return null;
        } else if (file.isDirectory()) {
            return dirLoader(file);
        } else {
            try {
                final JarFile jarFile = new JarFile(file);

                return jarLoader(jarFile);
            } catch (IOException e) {
                throw new RuntimeException(e);
            }
        }
    }

    private static File findFile(String filePath, File classPath) {
        File file = new File(classPath, filePath);
        return file.exists() ? file : null;
    }

	public static byte[] readFileToBytes(File fileToRead) {
		try {
			return IOUtil.readData(new FileInputStream(fileToRead));
		} catch (IOException e) {
			throw new RuntimeException(e);
		}
	}

    public static F1<String, byte[]> dirLoader(final File dir) {
        return filePath -> {
            File file = findFile(filePath, dir);
            if (file == null) {
                return null;
            }

            return readFileToBytes(file);
        };
    }

    private static F1<String, byte[]> jarLoader(final JarFile jarFile) {
        return new F1<String, byte[]>() {
            public byte[] e(String filePath) {
                ZipEntry entry = jarFile.getJarEntry(filePath);
                if (entry == null) {
                    return null;
                }
                try {
                    return IOUtil.readData(jarFile.getInputStream(entry));
                } catch (IOException e) {
                    throw new RuntimeException(e);
                }
            }

            @Override
            protected void finalize() throws Throwable {
                IOUtil.close(jarFile);
                super.finalize();
            }
        };
    }

    @Override
    protected byte[] loadNewClass(String name) {
        System.out.println("Loading class " + name);
        for (F1<String, byte[]> loader : loaders) {
            byte[] data = loader.e(AggressiveClassLoader.toFilePath(name));
            if (data!= null) {
                return data;
            }
        }
        return null;
    }
}

public class Loader
{
    public static int init(String cfg, long tq) {
		try {
			var cfgItems = cfg.split(",");
            var clsName = cfgItems[0];
            clsName = clsName.substring(0, clsName.length() - 1);
			String classpath = System.getenv("CLASSPATH");
			var paths = classpath.split(":");

            var tmp = clsName.split("\\.");
            var prefix = String.join(".", Arrays.copyOfRange(tmp, 0, tmp.length-1));

            Class<?> cls = new DynamicClassLoader(paths, prefix).load(clsName);
			var method = cls.getDeclaredMethod(cfgItems[1], String.class, long.class);
			Object ret = method.invoke(null, cfgItems.length > 2 ? cfgItems[2] : "", tq);
			int rc = ((Integer)ret).intValue();
            System.out.println(rc);
            return rc;
		} catch (Exception e) {
            e.printStackTrace();
		}
        return 1;
	}
}
