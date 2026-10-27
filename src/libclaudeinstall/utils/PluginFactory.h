/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2015 Teo Mrnjavac <teo@kde.org>
 *   SPDX-FileCopyrightText: 2017-2018 Adriaan de Groot <groot@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 *
 */

#ifndef UTILS_PLUGINFACTORY_H
#define UTILS_PLUGINFACTORY_H

#include "DllMacro.h"

#include <QObject>

#define ClaudeInstallPluginFactory_iid "io.claudeinstall.PluginFactory"

/** @brief Plugin factory for ClaudeInstall
 *
 * A ClaudeInstall plugin contains just one kind of plugin -- either
 * a job, or a viewstep -- so the factory is straightforward.
 * It gets a single CreateInstanceFunction and calls that;
 * the function is set when registerPlugin() is called in a subclass.
 *
 */
class DLLEXPORT ClaudeInstallPluginFactory : public QObject
{
    Q_OBJECT
public:
    explicit ClaudeInstallPluginFactory() {}
    ~ClaudeInstallPluginFactory() override;

    typedef QObject* ( *CreateInstanceFunction )( QObject* );

    template < class T >
    T* create( QObject* parent = nullptr )
    {
        auto* op = fn ? fn( parent ) : nullptr;
        if ( !op )
        {
            return nullptr;
        }
        T* tp = qobject_cast< T* >( op );
        if ( !tp )
        {
            delete op;
        }
        return tp;
    }

protected:
    CreateInstanceFunction fn = nullptr;
};

/** @brief declare a ClaudeInstall Plugin Factory
 *
 * There should be one declaration -- generally alongside the
 * class definition for the Job or ViewStep that the plugin is
 * going to provide, in the header -- and one definition -- in
 * the corresponding implementation.
 */
#define CLAUDEINSTALL_PLUGIN_FACTORY_DECLARATION( name ) \
    class name : public ClaudeInstallPluginFactory \
    { \
        Q_OBJECT \
        Q_INTERFACES( ClaudeInstallPluginFactory ) \
        Q_PLUGIN_METADATA( IID ClaudeInstallPluginFactory_iid ) \
    public: \
        explicit name(); \
        ~name() override; \
        template < class T > \
        static QObject* createInstance( QObject* parent ) \
        { \
            return new T( parent ); \
        } \
        template < class T > \
        void registerPlugin() \
        { \
            fn = createInstance< T >; \
        } \
    };

/** @brief Define a ClaudeInstall Plugin Factory
 *
 * This should be done exactly once, generally in the translation
 * unit containing the definitions for the main class of the plugin,
 * either the Job or the ViewStep definitions.
 *
 * The @p name must match the name used in the declaration, while
 * @p pluginRegistrations should be a single call to `registerPlugin<T>()`
 * where `T` is the type (subclass of Job or ViewStep) defined by the
 * plugin, eg.
 *
 * ```
 * CLAUDEINSTALL_PLUGIN_FACTORY_DEFINITION( MyPlugin, registerPlugin<MyPluginJob>() )
 * ```
 *
 * Leaving out the `()` will lead to generally-weird compiler warnings.
 */
#define CLAUDEINSTALL_PLUGIN_FACTORY_DEFINITION( name, pluginRegistrations ) \
    name::name() \
        : ClaudeInstallPluginFactory() \
    { \
        pluginRegistrations; \
    } \
    name::~name() {}

Q_DECLARE_INTERFACE( ClaudeInstallPluginFactory, ClaudeInstallPluginFactory_iid )

#endif
